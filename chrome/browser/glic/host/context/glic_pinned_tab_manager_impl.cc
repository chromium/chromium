// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pinned_tab_manager_impl.h"

#include <algorithm>
#include <functional>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_pin_candidate_comparator.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "glic_pinned_tab_manager.h"
#include "url/origin.h"

namespace glic {

namespace {
BASE_FEATURE(kGlicAutoUnpinOnTabChangedOrigin,
             base::FEATURE_ENABLED_BY_DEFAULT);

// An arbitrary limit.
const int32_t kDefaultMaxPinnedTabs = 5;

// A limit to use when the number set by the client is "unlimited". This is an
// arbitrary large number.
const int32_t kMaxPinnedTabs = 256;

bool IsForeground(content::Visibility visibility) {
  return visibility != content::Visibility::HIDDEN;
}

}  // namespace

class GlicPinnedTabManagerImpl::PinnedTabObserver
    : public content::WebContentsObserver {
 public:
  PinnedTabObserver(GlicPinnedTabManagerImpl* pinned_tab_manager,
                    tabs::TabInterface* tab,
                    GlicInstanceHelper& helper)
      : content::WebContentsObserver(tab->GetContents()),
        pinned_tab_manager_(pinned_tab_manager),
        tab_(tab) {
    will_discard_contents_subscription_ =
        tab_->RegisterWillDiscardContents(base::BindRepeating(
            &PinnedTabObserver::OnWillDiscardContents, base::Unretained(this)));
    on_destroy_subscription_ =
        helper.SubscribeToDestruction(base::BindRepeating(
            &PinnedTabObserver::OnTabDestroyed, base::Unretained(this)));
    StartObservation(tab, tab->GetContents());
    content::WebContents* web_contents = tab->GetContents();
    if (web_contents) {
      is_audible_ = web_contents->IsCurrentlyAudible();
      is_foreground_ = IsForeground(web_contents->GetVisibility());
      last_origin_ =
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    }
    did_become_visible_subscription_ =
        tab_->RegisterDidBecomeVisible(base::BindRepeating(
            &PinnedTabObserver::OnDidBecomeVisible, base::Unretained(this)));
    will_become_hidden_subscription_ =
        tab_->RegisterWillBecomeHidden(base::BindRepeating(
            &PinnedTabObserver::OnWillBecomeHidden, base::Unretained(this)));
  }
  ~PinnedTabObserver() override { ClearObservation(); }

  PinnedTabObserver(const PinnedTabObserver&) = delete;
  PinnedTabObserver& operator=(const PinnedTabObserver&) = delete;

  // content::WebContentsObserver.
  void OnAudioStateChanged(bool audible) override {
    bool was_observable = IsObservable();
    is_audible_ = audible;
    if (was_observable != IsObservable()) {
      UpdateTabDataAndSend(
          {{TabDataChangeCause::kAudioState}, CreateTabData(tab_)});
    }
  }

  void OnDidBecomeVisible(tabs::TabInterface* tab) {
    if (is_foreground_) {
      return;
    }
    is_foreground_ = true;
    UpdateTabDataAndSend(
        {{TabDataChangeCause::kVisibility}, CreateTabData(tab_)});
  }

  void OnWillBecomeHidden(tabs::TabInterface* tab) {
    if (!is_foreground_) {
      return;
    }
    is_foreground_ = false;
    // Post a task so that the update call happens after the tab is hidden. This
    // ensures that TabInterface::IsActivated() returns the right value.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<PinnedTabObserver> self) {
              if (self) {
                self->UpdateTabDataAndSend({{TabDataChangeCause::kVisibility},
                                            CreateTabData(self->tab_)});
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    // If the navigation is a restore, we update the origin but do not unpin the
    // tab. This allows restored tabs to load their content (changing origin
    // from empty/initial to the restored site) without triggering the privacy
    // check that unpins background tabs on origin change.
    if (navigation_handle->GetRestoreType() ==
        content::RestoreType::kRestored) {
      last_origin_ =
          navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin();
      return;
    }

    CheckOriginChangeAndMaybeDeleteSelf(
        navigation_handle->GetRenderFrameHost()->GetLastCommittedOrigin());
  }

  void OnTabDestroyed(tabs::TabInterface* tab) {
    CHECK_EQ(tab_, tab);
    ClearObservation();
    pinned_tab_manager_->OnTabWillClose(tab_->GetHandle());
  }

  void OnWillDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents) {
    CHECK_EQ(web_contents(), old_contents);
    StartObservation(tab, new_contents);
    // When a tab is discarded, the new contents (placeholder) might have an
    // opaque origin. We update the last_origin_ but do not trigger unpinning
    // checks.
    last_origin_ =
        new_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  }

  void FocusedTabDataChanged(TabDataChange tab_data) {
    UpdateTabDataAndSend(std::move(tab_data));
  }

  bool IsObservable() const { return is_foreground_ || is_audible_; }

 private:
  void CheckOriginChangeAndMaybeDeleteSelf(const url::Origin& new_origin) {
    if (last_origin_ == new_origin) {
      return;
    }
    last_origin_ = new_origin;
    base::RecordAction(base::UserMetricsAction("Glic.PinnedTab.OriginChanged"));
    // May delete this.
    pinned_tab_manager_->OnTabChangedOrigin(tab_->GetHandle());
  }

  void UpdateTabDataAndSend(TabDataChange change) {
    // Add observability info.
    change.tab_data->is_observable = IsObservable();
    pinned_tab_manager_->OnTabDataChanged(tab_->GetHandle(), std::move(change));
  }

  void StartObservation(tabs::TabInterface* tab,
                        content::WebContents* contents) {
    Observe(contents);
    tab_data_observer_ = std::make_unique<TabDataObserver>(
        tab, contents,
        base::BindRepeating(&PinnedTabObserver::FocusedTabDataChanged,
                            base::Unretained(this)));
  }

  void ClearObservation() {
    Observe(nullptr);
    tab_data_observer_.reset();
  }

  // Owns this.
  raw_ptr<GlicPinnedTabManagerImpl> pinned_tab_manager_;
  raw_ptr<tabs::TabInterface> tab_;

  base::CallbackListSubscription will_discard_contents_subscription_;
  base::CallbackListSubscription on_destroy_subscription_;
  base::CallbackListSubscription did_become_visible_subscription_;
  base::CallbackListSubscription will_become_hidden_subscription_;

  bool is_foreground_ = false;
  bool is_audible_ = false;
  url::Origin last_origin_;

  std::unique_ptr<TabDataObserver> tab_data_observer_;

  base::WeakPtrFactory<PinnedTabObserver> weak_ptr_factory_{this};
};

GlicPinnedTabManagerImpl::PinnedTabEntry::PinnedTabEntry(
    tabs::TabHandle tab_handle,
    std::unique_ptr<PinnedTabObserver> tab_observer,
    GlicPinnedTabUsage usage)
    : tab_handle(tab_handle),
      tab_observer(std::move(tab_observer)),
      usage(usage) {}

GlicPinnedTabManagerImpl::PinnedTabEntry::~PinnedTabEntry() = default;

GlicPinnedTabManagerImpl::PinnedTabEntry::PinnedTabEntry(
    PinnedTabEntry&& other) = default;

GlicPinnedTabManagerImpl::PinnedTabEntry&
GlicPinnedTabManagerImpl::PinnedTabEntry::operator=(PinnedTabEntry&& other) =
    default;

// A helper class to throttle updates using exponential backoff. It coalesces
// multiple requests into a single callback execution. The delay increases
// exponentially when updates are frequent and resets to an initial value after
// a quiet period (i.e. when a timer fires without any new requests having
// been queued).
class GlicPinnedTabManagerImpl::UpdateThrottler {
 public:
  explicit UpdateThrottler(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {}
  ~UpdateThrottler() = default;

  void RequestUpdate() {
    if (timer_.IsRunning()) {
      pending_update_ = true;
      return;
    }

    timer_.Start(FROM_HERE, current_delay_, this,
                 &UpdateThrottler::OnTimerFired);
  }

 private:
  static constexpr base::TimeDelta kInitialDelay = base::Milliseconds(50);
  static constexpr base::TimeDelta kMaxDelay = base::Milliseconds(250);
  static constexpr double kMultiplier = 2.0;

  void OnTimerFired() {
    callback_.Run();

    if (pending_update_) {
      pending_update_ = false;
      current_delay_ = std::min(current_delay_ * kMultiplier, kMaxDelay);
      timer_.Start(FROM_HERE, current_delay_, this,
                   &UpdateThrottler::OnTimerFired);
    } else {
      current_delay_ = kInitialDelay;
    }
  }

  base::RepeatingClosure callback_;
  base::OneShotTimer timer_;
  bool pending_update_ = false;
  base::TimeDelta current_delay_ = kInitialDelay;
};

GlicPinnedTabManagerImpl::GlicPinnedTabManagerImpl(
    Profile* profile,
    GlicInstance::UIDelegate* ui_delegate,
    GlicMetrics* metrics)
    : profile_(profile),
      ui_delegate_(ui_delegate),
      metrics_(metrics),
      max_pinned_tabs_(kDefaultMaxPinnedTabs) {
  pin_candidate_updater_ = std::make_unique<UpdateThrottler>(
      base::BindRepeating(&GlicPinnedTabManagerImpl::SendPinCandidatesUpdate,
                          weak_ptr_factory_.GetWeakPtr()));
}

GlicPinnedTabManagerImpl::~GlicPinnedTabManagerImpl() = default;

base::CallbackListSubscription
GlicPinnedTabManagerImpl::AddPinnedTabsChangedCallback(
    PinnedTabsChangedCallback callback) {
  return pinned_tabs_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicPinnedTabManagerImpl::AddPinnedTabDataChangedCallback(
    PinnedTabDataChangedCallback callback) {
  return pinned_tab_data_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicPinnedTabManagerImpl::AddTabPinningStatusChangedCallback(
    TabPinningStatusChangedCallback callback) {
  return pinning_status_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicPinnedTabManagerImpl::AddTabPinningStatusEventCallback(
    TabPinningStatusEventCallback callback) {
  return pinning_status_event_callback_list_.Add(std::move(callback));
}

bool GlicPinnedTabManagerImpl::PinTabs(
    base::span<const tabs::TabHandle> tab_handles,
    GlicPinTrigger trigger) {
  base::TimeTicks pin_timestamp = base::TimeTicks::Now();

  bool pinning_fully_succeeded = true;
  for (const auto tab_handle : tab_handles) {
    if (pinned_tabs_.size() == static_cast<size_t>(max_pinned_tabs_)) {
      pinning_fully_succeeded = false;
      metrics_->OnTabPinnedForSharing(
          GlicTabPinnedForSharingResult::kPinTabForSharingFailedTooManyTabs);
      continue;
    }
    auto* tab = tab_handle.Get();
    if (!tab || IsTabPinned(tab_handle) ||
        !IsBrowserValidForSharing(tab->GetBrowserWindowInterface())) {
      pinning_fully_succeeded = false;
      metrics_->OnTabPinnedForSharing(
          GlicTabPinnedForSharingResult::
              kPinTabForSharingFailedNotValidForSharing);
      continue;
    }

    // Tab might be unloaded (e.g. discarded, restored from history). We reload
    // it now (and prevent it from being discarded elsewhere), so it can have
    // its context pulled.
    if (tab->GetContents()) {
      if (tab->GetContents()->WasDiscarded()) {
        tab->GetContents()->GetController().SetNeedsReload();
      }
      tab->GetContents()->GetController().LoadIfNecessary();
    }

    GlicInstanceHelper* helper = GlicInstanceHelper::From(tab);
    if (!helper) {
      LOG(WARNING)
          << "Tab not pinned because it didn't have a GlicInstanceHelper";
      continue;
    }

    GlicPinnedTabUsage usage = GlicPinnedTabUsage(trigger, pin_timestamp);
    pinned_tabs_.emplace_back(
        tab_handle,
        std::make_unique<PinnedTabObserver>(this, tab_handle.Get(), *helper),
        usage);
    pinning_status_event_callback_list_.Notify(tab_handle.Get(),
                                               usage.pin_event);
    pinning_status_changed_callback_list_.Notify(tab_handle.Get(), true);
    metrics_->OnTabPinnedForSharing(
        GlicTabPinnedForSharingResult::kPinTabForSharingSucceeded);
  }
  NotifyPinnedTabsChanged();
  return pinning_fully_succeeded;
}

bool GlicPinnedTabManagerImpl::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles,
    GlicUnpinTrigger trigger) {
  base::TimeTicks unpin_timestamp = base::TimeTicks::Now();

  bool unpinning_fully_succeeded = true;
  for (const auto tab_handle : tab_handles) {
    auto* tab = tab_handle.Get();
    auto* entry = GetPinnedTabEntry(tab_handle);
    if (!tab || !entry) {
      unpinning_fully_succeeded = false;
      continue;
    }
    GlicUnpinEvent unpin_event =
        GlicUnpinEvent(trigger, entry->usage, unpin_timestamp);
    std::erase_if(pinned_tabs_, [tab_handle](const PinnedTabEntry& entry) {
      return entry.tab_handle == tab_handle;
    });
    pinning_status_event_callback_list_.Notify(tab_handle.Get(), unpin_event);
    pinning_status_changed_callback_list_.Notify(tab_handle.Get(), false);
  }
  NotifyPinnedTabsChanged();
  return unpinning_fully_succeeded;
}

void GlicPinnedTabManagerImpl::UnpinAllTabs(GlicUnpinTrigger trigger) {
  std::vector<tabs::TabHandle> tabs_to_unpin;
  for (auto& entry : pinned_tabs_) {
    tabs_to_unpin.push_back(entry.tab_handle);
  }
  UnpinTabs(tabs_to_unpin, trigger);
}

const GlicPinnedTabManagerImpl::PinnedTabEntry*
GlicPinnedTabManagerImpl::GetPinnedTabEntry(tabs::TabHandle tab_handle) const {
  auto it = std::find_if(pinned_tabs_.begin(), pinned_tabs_.end(),
                         [tab_handle](const PinnedTabEntry& entry) {
                           return entry.tab_handle == tab_handle;
                         });
  if (it == pinned_tabs_.end()) {
    return nullptr;
  }
  return &(*it);
}

GlicPinnedTabUsage* GlicPinnedTabManagerImpl::GetPinnedTabUsageInternal(
    tabs::TabHandle tab_handle) {
  auto it = std::find_if(pinned_tabs_.begin(), pinned_tabs_.end(),
                         [tab_handle](const PinnedTabEntry& entry) {
                           return entry.tab_handle == tab_handle;
                         });
  if (it == pinned_tabs_.end()) {
    return nullptr;
  }

  return &it->usage;
}

uint32_t GlicPinnedTabManagerImpl::SetMaxPinnedTabs(uint32_t max_pinned_tabs) {
  if (max_pinned_tabs < GetNumPinnedTabs()) {
    max_pinned_tabs = GetNumPinnedTabs();
  } else if (max_pinned_tabs > kMaxPinnedTabs) {
    max_pinned_tabs = kMaxPinnedTabs;
  }
  max_pinned_tabs_ = max_pinned_tabs;
  return max_pinned_tabs;
}

uint32_t GlicPinnedTabManagerImpl::GetMaxPinnedTabs() const {
  return max_pinned_tabs_;
}

uint32_t GlicPinnedTabManagerImpl::GetNumPinnedTabs() const {
  return static_cast<uint32_t>(pinned_tabs_.size());
}

bool GlicPinnedTabManagerImpl::IsTabPinned(tabs::TabHandle tab_handle) const {
  return !!GetPinnedTabEntry(tab_handle);
}

std::vector<content::WebContents*> GlicPinnedTabManagerImpl::GetPinnedTabs()
    const {
  std::vector<content::WebContents*> pinned_contents;
  for (auto& entry : pinned_tabs_) {
    pinned_contents.push_back(entry.tab_observer->web_contents());
  }
  return pinned_contents;
}

std::optional<GlicPinnedTabUsage> GlicPinnedTabManagerImpl::GetPinnedTabUsage(
    tabs::TabHandle tab_handle) const {
  const auto* entry = GetPinnedTabEntry(tab_handle);
  if (!entry) {
    return std::nullopt;
  }
  return entry->usage;
}

void GlicPinnedTabManagerImpl::SubscribeToPinCandidates(
    mojom::GetPinCandidatesOptionsPtr options,
    mojo::PendingRemote<mojom::PinCandidatesObserver> observer) {
  pin_candidates_observer_.reset();
  pin_candidates_observer_.Bind(std::move(observer));
  pin_candidates_observer_.set_disconnect_handler(base::BindOnce(
      &GlicPinnedTabManagerImpl::OnPinCandidatesObserverDisconnected,
      base::Unretained(this)));
  pin_candidates_options_ = std::move(options);
  pin_candidate_updater_->RequestUpdate();
  tab_observer_ = GlicTabObserver::Create(
      profile_, base::BindRepeating(&GlicPinnedTabManagerImpl::OnTabEvent,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void GlicPinnedTabManagerImpl::OnPinnedTabContextEvent(
    tabs::TabHandle tab_handle,
    GlicPinnedTabContextEvent context_event) {
  auto* pinned_usage = GetPinnedTabUsageInternal(tab_handle);
  if (!pinned_usage) {
    return;
  }
  OnPinnedTabContextEvent(*pinned_usage, context_event);
}

void GlicPinnedTabManagerImpl::OnPinnedTabContextEvent(
    GlicPinnedTabUsage& pinned_usage,
    GlicPinnedTabContextEvent context_event) {
  switch (context_event.type) {
    case GlicPinnedTabContextEventType::kConversationTurnSubmitted:
      pinned_usage.times_conversation_turn_submitted_while_pinned++;
      break;
  }
}

void GlicPinnedTabManagerImpl::OnAllPinnedTabsContextEvent(
    GlicPinnedTabContextEvent context_event) {
  for (auto& entry : pinned_tabs_) {
    OnPinnedTabContextEvent(entry.usage, context_event);
  }
}

void GlicPinnedTabManagerImpl::SendPinCandidatesUpdate() {
  if (!pin_candidates_observer_) {
    return;
  }

  std::vector<content::WebContents*> candidates = GetUnsortedPinCandidates();
  GlicPinCandidateComparator comparator(pin_candidates_options_->query);
  size_t limit =
      std::min(static_cast<size_t>(pin_candidates_options_->max_candidates),
               candidates.size());
  std::partial_sort(candidates.begin(), candidates.begin() + limit,
                    candidates.end(), std::ref(comparator));
  std::vector<mojom::PinCandidatePtr> results;
  for (size_t i = 0; i < limit; ++i) {
    results.push_back(mojom::PinCandidate::New(
        CreateTabData(tabs::TabInterface::GetFromContents(candidates[i]))));
  }
  pin_candidates_observer_->OnPinCandidatesChanged(std::move(results));
}

std::vector<content::WebContents*>
GlicPinnedTabManagerImpl::GetUnsortedPinCandidates() {
  std::vector<content::WebContents*> candidates;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, &candidates](BrowserWindowInterface* browser_window_interface) {
        if (browser_window_interface->GetProfile() != profile_ ||
            browser_window_interface->GetType() !=
                BrowserWindowInterface::Type::TYPE_NORMAL) {
          return true;
        }
        auto all_tabs =
            TabListInterface::From(browser_window_interface)->GetAllTabs();
        for (auto* tab : all_tabs) {
          if (IsTabPinned(tab->GetHandle())) {
            continue;
          }
          if (!IsBrowserValidForSharing(tab->GetBrowserWindowInterface())) {
            continue;
          }
          auto* web_contents = tab->GetContents();
          if (!web_contents->GetController().GetLastCommittedEntry()) {
            continue;
          }
          if (!IsValidForSharing(web_contents)) {
            continue;
          }
          candidates.push_back(web_contents);
        }
        return true;
      });
  return candidates;
}

void GlicPinnedTabManagerImpl::OnTabEvent(const GlicTabEvent& event) {
  if (!pin_candidates_observer_) {
    return;
  }
  // All events signal a need to update candidates.
  pin_candidate_updater_->RequestUpdate();
}

void GlicPinnedTabManagerImpl::OnPinCandidatesObserverDisconnected() {
  pin_candidates_observer_.reset();
  tab_observer_.reset();
}

void GlicPinnedTabManagerImpl::NotifyPinnedTabsChanged() {
  pinned_tabs_changed_callback_list_.Notify(GetPinnedTabs());
}

void GlicPinnedTabManagerImpl::OnTabDataChanged(tabs::TabHandle tab_handle,
                                                TabDataChange tab_data_change) {
  CHECK(IsTabPinned(tab_handle));
  pinned_tab_data_changed_callback_list_.Notify(tab_data_change);
}

void GlicPinnedTabManagerImpl::OnTabChangedOrigin(tabs::TabHandle tab_handle) {
  CHECK(IsTabPinned(tab_handle));
  if ((!GlicEnabling::IsMultiInstanceEnabled() ||
       base::FeatureList::IsEnabled(kGlicAutoUnpinOnTabChangedOrigin)) &&
      !IsGlicWindowShowing()) {
    // If the tab was restored, we do not unpin it when the origin changes.
    // TODO(b/456482198): Find a more nuanced solution that doesn't permanently
    // prevent background unpinning for restored tabs.
    auto* usage = GetPinnedTabUsageInternal(tab_handle);
    if (usage && usage->pin_event.trigger == GlicPinTrigger::kRestore) {
      return;
    }

    base::RecordAction(
        base::UserMetricsAction("Glic.PinnedTab.OriginChanged.Unpinned"));
    UnpinTabs({tab_handle}, GlicUnpinTrigger::kBackgroundTabNavigation);
  }
}

void GlicPinnedTabManagerImpl::OnTabWillClose(tabs::TabHandle tab_handle) {
  // TODO(b/426644733): Avoid n^2 work when closing all tabs.
  CHECK(UnpinTabs({tab_handle}, GlicUnpinTrigger::kTabClose));
  NotifyPinnedTabsChanged();
}

bool GlicPinnedTabManagerImpl::IsBrowserValidForSharing(
    BrowserWindowInterface* browser_window) {
  return IsBrowserValidForSharingInProfile(browser_window, profile_);
}

bool GlicPinnedTabManagerImpl::IsValidForSharing(
    content::WebContents* web_contents) {
  return IsTabValidForSharing(web_contents);
}

bool GlicPinnedTabManagerImpl::IsGlicWindowShowing() {
  return ui_delegate_->IsShowing();
}

}  // namespace glic
