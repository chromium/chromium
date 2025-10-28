// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"
#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/appkit_utils.h"
#endif

namespace glic {

namespace {

// Returns whether `a` and `b` both point to the same object.
// Note that if both `a` and `b` are invalidated, this returns true, even if
// the object they once pointed to is different. For our purposes, this is OK.
// This code helps address focus state changes from an old state that's since
// been invalidated to a new state that is now nullptr (we want to treat this
// as a "focus changed" scenario and notify).
template <typename T>
bool IsWeakPtrSame(const base::WeakPtr<T>& a, const base::WeakPtr<T>& b) {
  return std::make_pair(a.get(), a.WasInvalidated()) ==
         std::make_pair(b.get(), b.WasInvalidated());
}

}  // namespace

GlicFocusedTabManager::GlicFocusedTabManager(
    GlicFocusedBrowserManager* focused_browser_manager)
    : focused_browser_manager_(focused_browser_manager) {}

GlicFocusedTabManager::~GlicFocusedTabManager() = default;

void GlicFocusedTabManager::Initialize() {
  focused_browser_subscription_ =
      focused_browser_manager_->AddFocusedBrowserChangedCallback(
          base::BindRepeating(&GlicFocusedTabManager::OnFocusedBrowserChanged,
                              base::Unretained(this)));
  OnFocusedBrowserChanged(focused_browser_manager_->GetFocusedBrowser(),
                          focused_browser_manager_->GetCandidateBrowser());
  MaybeUpdateFocusedTab();
}

base::CallbackListSubscription
GlicFocusedTabManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  if (!focused_browser_subscription_) {
    Initialize();
  }
  return focused_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicFocusedTabManager::AddFocusedTabInstanceChangedCallback(
    FocusedTabInstanceChangedCallback callback) {
  if (!focused_browser_subscription_) {
    Initialize();
  }
  return focused_instance_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicFocusedTabManager::AddFocusedTabOrCandidateInstanceChangedCallback(
    FocusedTabOrCandidateInstanceChangedCallback callback) {
  if (!focused_browser_subscription_) {
    Initialize();
  }
  return focused_or_candidate_instance_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicFocusedTabManager::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  if (!focused_browser_subscription_) {
    Initialize();
  }
  return focused_data_callback_list_.Add(std::move(callback));
}

void GlicFocusedTabManager::OnFocusedBrowserChanged(
    BrowserWindowInterface* candidate,
    BrowserWindowInterface* focused) {
  BrowserWindowInterface* previously_subscribed = subscribed_browser_.get();
  if (previously_subscribed) {
    previously_subscribed->GetTabStripModel()->RemoveObserver(this);
  }
  if (focused) {
    focused->GetTabStripModel()->AddObserver(this);
    subscribed_browser_ = focused->GetWeakPtr();
    active_tab_subscription_ = focused->RegisterActiveTabDidChange(
        base::BindRepeating(&GlicFocusedTabManager::OnActiveTabChanged,
                            base::Unretained(this)));
  } else {
    subscribed_browser_.reset();
    active_tab_subscription_ = base::CallbackListSubscription();
  }

  // We need to force-notify because even if the focused tab doesn't change, it
  // can be in a different browser window (i.e., the user drag-n-drop the
  // focused tab into a new window). Let the subscribers to decide what to do in
  // this case.
  //
  // TODO(crbug.com/393578218): We should have dedicated subscription lists for
  // different types of notifications.
  MaybeUpdateFocusedTab(/*force_notify=*/true);
}

void GlicFocusedTabManager::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kContentsChanged) {
    MaybeUpdateFocusedTab(/*force_notify=*/true);
  }
}

void GlicFocusedTabManager::PrimaryPageChanged(content::Page& page) {
  // We always want to trigger our notify callback here (even if focused tab
  // remains the same) so that subscribers can update if they care about primary
  // page changed events.
  MaybeUpdateFocusedTab(/*force_notify=*/true);
}

void GlicFocusedTabManager::FocusedTabDataChanged(TabDataChange change) {
  // `TabDataObserver` is responsible for firing this when appropriate, we just
  // forward events along.
  // Note: we omit calling `MaybeUpdateFocusedTab()` here because observing web
  // contents for changes that might impact focused tab container or candidate
  // are handled separately.
  NotifyFocusedTabDataChanged(std::move(change));
}

void GlicFocusedTabManager::MaybeUpdateFocusedTab(bool force_notify) {
  struct FocusedTabState new_focused_tab_state = ComputeFocusedTabState();
  bool focus_changed = !focused_tab_state_.IsSame(new_focused_tab_state);
  bool focused_instance_changed = !IsWeakPtrSame(
      focused_tab_state_.focused_tab, new_focused_tab_state.focused_tab);
  bool focused_or_candidate_instance_changed =
      focused_instance_changed ||
      !IsWeakPtrSame(focused_tab_state_.candidate_tab,
                     new_focused_tab_state.candidate_tab);
  if (focus_changed) {
    focused_tab_state_ = new_focused_tab_state;
    focused_tab_data_ = GetFocusedTabData(new_focused_tab_state);
  }

  // If we have one, observe tab candidate. If not, whether that's because there
  // was never one, or because it's been invalidated, turn off tab candidate
  // observation.
  Observe(focused_tab_state_.candidate_tab.get());

  // Similarly set up or turn off tab data observation for the focused tab.
  focused_tab_data_observer_ = std::make_unique<TabDataObserver>(
      focused_tab_state_.focused_tab.get(),
      base::BindRepeating(&GlicFocusedTabManager::FocusedTabDataChanged,
                          base::Unretained(this)));

  if (focused_instance_changed) {
    NotifyFocusedTabInstanceChanged(focused_tab_state_.focused_tab.get());
    NotifyFocusedTabDataChanged(
        {{TabDataChangeCause::kTabChanged},
         CreateTabData(focused_tab_state_.focused_tab.get())});
  }

  if (focused_or_candidate_instance_changed) {
    NotifyFocusedTabOrCandidateInstanceChanged(ImplToPublic(focused_tab_data_));
  }

  if (focus_changed || force_notify) {
    NotifyFocusedTabChanged();
  }
}

struct GlicFocusedTabManager::FocusedTabState
GlicFocusedTabManager::ComputeFocusedTabState() {
  struct FocusedTabState focused_tab_state = FocusedTabState();

  BrowserWindowInterface* candidate_browser =
      focused_browser_manager_->GetCandidateBrowser();
  if (!candidate_browser) {
    return focused_tab_state;
  }
  focused_tab_state.candidate_browser = candidate_browser->GetWeakPtr();

  BrowserWindowInterface* focused_browser =
      focused_browser_manager_->GetFocusedBrowser();
  if (focused_browser) {
    focused_tab_state.focused_browser = focused_browser->GetWeakPtr();
    CHECK_EQ(focused_browser, candidate_browser);
  }

  content::WebContents* active_contents =
      candidate_browser->GetActiveTabInterface()
          ? candidate_browser->GetActiveTabInterface()->GetContents()
          : nullptr;
  if (active_contents) {
    focused_tab_state.candidate_tab = active_contents->GetWeakPtr();
  }
  if (candidate_browser == focused_browser && active_contents &&
      IsTabValidForSharing(active_contents)) {
    focused_tab_state.focused_tab = focused_tab_state.candidate_tab;
  }

  return focused_tab_state;
}

void GlicFocusedTabManager::NotifyFocusedTabChanged() {
  focused_callback_list_.Notify(GetFocusedTabData());
}

void GlicFocusedTabManager::NotifyFocusedTabInstanceChanged(
    content::WebContents* web_contents) {
  focused_instance_callback_list_.Notify(web_contents);
}

void GlicFocusedTabManager::NotifyFocusedTabOrCandidateInstanceChanged(
    const FocusedTabData& focused_tab_data) {
  focused_or_candidate_instance_callback_list_.Notify(focused_tab_data);
}

void GlicFocusedTabManager::NotifyFocusedTabDataChanged(TabDataChange change) {
  focused_data_callback_list_.Notify(change.tab_data.get());
}

bool GlicFocusedTabManager::IsTabFocused(tabs::TabHandle tab_handle) const {
  auto* tab = tab_handle.Get();
  if (!tab) {
    return false;
  }
  content::WebContents* web_contents = focused_tab_data_.focus();
  if (!web_contents) {
    return false;
  }
  return tab->GetContents() == web_contents;
}

GlicFocusedTabManager::FocusedTabDataImpl
GlicFocusedTabManager::GetFocusedTabData(
    const GlicFocusedTabManager::FocusedTabState& focused_state) {
  if (focused_state.focused_tab) {
    return FocusedTabDataImpl(focused_state.focused_tab);
  }

  if (focused_state.candidate_tab) {
    return FocusedTabDataImpl(NoFocusedTabData(
        "no focusable tab", focused_state.candidate_tab.get()));
  }

  if (focused_state.focused_browser) {
    return FocusedTabDataImpl(NoFocusedTabData("no focusable tab"));
  }

  if (focused_state.candidate_browser) {
    return FocusedTabDataImpl(NoFocusedTabData("no focusable browser window"));
  }

  return FocusedTabDataImpl(NoFocusedTabData("no browser window"));
}

FocusedTabData GlicFocusedTabManager::GetFocusedTabData() {
  return ImplToPublic(focused_tab_data_);
}

FocusedTabData GlicFocusedTabManager::ImplToPublic(FocusedTabDataImpl impl) {
  if (impl.is_focus()) {
    content::WebContents* contents = impl.focus();
    if (!contents) {
      return FocusedTabData(std::string("focused tab disappeared"),
                            /*unfocused_tab=*/nullptr);
    }
    return FocusedTabData(tabs::TabInterface::GetFromContents(contents));
  }
  const NoFocusedTabData* no_focus = impl.no_focus();
  CHECK(no_focus);
  content::WebContents* contents = no_focus->active_tab.get();
  tabs::TabInterface* tab =
      contents ? tabs::TabInterface::GetFromContents(contents) : nullptr;
  return FocusedTabData(std::string(no_focus->no_focus_reason), tab);
}

GlicFocusedTabManager::FocusedTabState::FocusedTabState() = default;
GlicFocusedTabManager::FocusedTabState::~FocusedTabState() = default;
GlicFocusedTabManager::FocusedTabState::FocusedTabState(
    const GlicFocusedTabManager::FocusedTabState& src) = default;
GlicFocusedTabManager::FocusedTabState&
GlicFocusedTabManager::FocusedTabState::operator=(
    const GlicFocusedTabManager::FocusedTabState& src) = default;

bool GlicFocusedTabManager::FocusedTabState::IsSame(
    const FocusedTabState& other) const {
  return IsWeakPtrSame(candidate_browser, other.candidate_browser) &&
         IsWeakPtrSame(focused_browser, other.focused_browser) &&
         IsWeakPtrSame(candidate_tab, other.candidate_tab) &&
         IsWeakPtrSame(focused_tab, other.focused_tab);
}

GlicFocusedTabManager::FocusedTabDataImpl::FocusedTabDataImpl(
    base::WeakPtr<content::WebContents> contents)
    : data_(std::move(contents)) {}

GlicFocusedTabManager::FocusedTabDataImpl::FocusedTabDataImpl(
    const NoFocusedTabData& no_focused_tab_data)
    : data_(no_focused_tab_data) {}

GlicFocusedTabManager::FocusedTabDataImpl::~FocusedTabDataImpl() = default;

GlicFocusedTabManager::FocusedTabDataImpl::FocusedTabDataImpl(
    const FocusedTabDataImpl& other) = default;

bool GlicFocusedTabManager::FocusedTabDataImpl::IsSame(
    const FocusedTabDataImpl& new_data) const {
  if (data_.index() != new_data.data_.index()) {
    return false;
  }
  switch (data_.index()) {
    case 0:
      return IsWeakPtrSame(std::get<0>(data_), std::get<0>(new_data.data_));
    case 1:
      return std::get<1>(data_).IsSame(std::get<1>(new_data.data_));
  }
  NOTREACHED();
}

bool GlicFocusedTabManager::NoFocusedTabData::IsSame(
    const NoFocusedTabData& other) const {
  return IsWeakPtrSame(active_tab, other.active_tab) &&
         no_focus_reason == other.no_focus_reason;
}

GlicFocusedTabManager::NoFocusedTabData::NoFocusedTabData() = default;
GlicFocusedTabManager::NoFocusedTabData::NoFocusedTabData(
    std::string_view reason,
    content::WebContents* tab)
    : active_tab(tab ? tab->GetWeakPtr() : nullptr), no_focus_reason(reason) {}
GlicFocusedTabManager::NoFocusedTabData::~NoFocusedTabData() = default;
GlicFocusedTabManager::NoFocusedTabData::NoFocusedTabData(
    const NoFocusedTabData& other) = default;
GlicFocusedTabManager::NoFocusedTabData&
GlicFocusedTabManager::NoFocusedTabData::operator=(
    const NoFocusedTabData& other) = default;

GlicPinAwareDetachedFocusedTabManager::GlicPinAwareDetachedFocusedTabManager(
    GlicSharingManager* sharing_manager,
    GlicFocusedBrowserManager* focused_browser_manager)
    : sharing_manager_(sharing_manager),
      focused_tab_manager_(focused_browser_manager) {}

GlicPinAwareDetachedFocusedTabManager::
    ~GlicPinAwareDetachedFocusedTabManager() = default;

void GlicPinAwareDetachedFocusedTabManager::InitializeSubscriptions() {
  focused_tab_changed_subscription_ =
      focused_tab_manager_.AddFocusedTabChangedCallback(base::BindRepeating(
          &GlicPinAwareDetachedFocusedTabManager::OnFocusedTabChanged,
          base::Unretained(this)));
  focused_tab_data_changed_subscription_ =
      focused_tab_manager_.AddFocusedTabDataChangedCallback(base::BindRepeating(
          &GlicPinAwareDetachedFocusedTabManager::OnFocusedTabDataChanged,
          base::Unretained(this)));
  tab_pinning_status_changed_subscription_ =
      sharing_manager_->AddTabPinningStatusChangedCallback(base::BindRepeating(
          &GlicPinAwareDetachedFocusedTabManager::OnTabPinningStatusChanged,
          base::Unretained(this)));
}

base::CallbackListSubscription
GlicPinAwareDetachedFocusedTabManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  if (!focused_tab_changed_subscription_) {
    InitializeSubscriptions();
  }
  return focused_tab_changed_callback_list_.Add(std::move(callback));
}

FocusedTabData GlicPinAwareDetachedFocusedTabManager::GetFocusedTabData() {
  return GetPinAwareFocusedTabData(focused_tab_manager_.GetFocusedTabData());
}

base::CallbackListSubscription
GlicPinAwareDetachedFocusedTabManager::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  if (!focused_tab_data_changed_subscription_) {
    InitializeSubscriptions();
  }
  return focused_tab_data_changed_callback_list_.Add(std::move(callback));
}

bool GlicPinAwareDetachedFocusedTabManager::IsTabFocused(
    tabs::TabHandle tab_handle) const {
  return focused_tab_manager_.IsTabFocused(tab_handle) &&
         sharing_manager_->IsTabPinned(tab_handle);
}

FocusedTabData GlicPinAwareDetachedFocusedTabManager::GetPinAwareFocusedTabData(
    const FocusedTabData& focused_tab_data) {
  if (focused_tab_data.focus() &&
      !sharing_manager_->IsTabPinned(focused_tab_data.focus()->GetHandle())) {
    return FocusedTabData(std::string("no focusable tab"),
                          focused_tab_data.focus());
  }

  if (focused_tab_data.focus()) {
    return FocusedTabData(focused_tab_data.focus());
  }

  return FocusedTabData(focused_tab_data.GetFocus().error(),
                        focused_tab_data.unfocused_tab());
}

void GlicPinAwareDetachedFocusedTabManager::OnFocusedTabChanged(
    const FocusedTabData& focused_tab_data) {
  NotifyFocusedTabChanged(GetPinAwareFocusedTabData(focused_tab_data));
}

void GlicPinAwareDetachedFocusedTabManager::OnFocusedTabDataChanged(
    const glic::mojom::TabData* focused_tab_data) {
  tabs::TabInterface* focused_tab =
      focused_tab_manager_.GetFocusedTabData().focus();
  if (focused_tab && !sharing_manager_->IsTabPinned(focused_tab->GetHandle())) {
    NotifyFocusedTabDataChanged(CreateTabData(nullptr).get());
    return;
  }

  NotifyFocusedTabDataChanged(focused_tab_data);
}

void GlicPinAwareDetachedFocusedTabManager::OnTabPinningStatusChanged(
    tabs::TabInterface* tab,
    bool pinned) {
  FocusedTabData focused_tab_data = focused_tab_manager_.GetFocusedTabData();
  // Tab should be non-null, so this check implies focus is set.
  if (tab == focused_tab_data.focus()) {
    FocusedTabData pin_aware_focused_tab_data =
        GetPinAwareFocusedTabData(focused_tab_data);
    NotifyFocusedTabChanged(pin_aware_focused_tab_data);
    NotifyFocusedTabDataChanged(
        CreateTabData(pin_aware_focused_tab_data.focus()
                          ? pin_aware_focused_tab_data.focus()->GetContents()
                          : nullptr)
            .get());
  }
}

void GlicPinAwareDetachedFocusedTabManager::NotifyFocusedTabChanged(
    const FocusedTabData& focused_tab) {
  focused_tab_changed_callback_list_.Notify(focused_tab);
}

void GlicPinAwareDetachedFocusedTabManager::NotifyFocusedTabDataChanged(
    const glic::mojom::TabData* focused_tab_data) {
  focused_tab_data_changed_callback_list_.Notify(focused_tab_data);
}

}  // namespace glic
