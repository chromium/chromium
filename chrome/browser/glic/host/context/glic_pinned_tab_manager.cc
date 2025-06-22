// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "components/prefs/pref_service.h"

namespace glic {

namespace {

// An arbitrary limit.
const int32_t kDefaultMaxPinnedTabs = 5;

// A limit to use when the number set by the client is "unlimited". This is an
// arbitrary large number.
const int32_t kMaxPinnedTabs = 256;

bool IsForeground(content::Visibility visibility) {
  return visibility != content::Visibility::HIDDEN;
}

}  // namespace

class GlicPinnedTabManager::PinnedTabObserver
    : public content::WebContentsObserver {
 public:
  PinnedTabObserver(
      tabs::TabInterface* tab,
      base::RepeatingCallback<void(tabs::TabHandle, glic::mojom::TabDataPtr)>
          tab_data_changed,
      base::OnceCallback<void(tabs::TabHandle)> tab_will_close)
      : content::WebContentsObserver(tab->GetContents()),
        tab_(tab),
        tab_data_changed_(std::move(tab_data_changed)),
        tab_will_close_(std::move(tab_will_close)) {
    will_discard_contents_subscription_ =
        tab_->RegisterWillDiscardContents(base::BindRepeating(
            &PinnedTabObserver::OnWillDiscardContents, base::Unretained(this)));
    will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
        &PinnedTabObserver::OnWillDetach, base::Unretained(this)));
    StartObservation(tab->GetContents());
    content::WebContents* web_contents = tab->GetContents();
    if (web_contents) {
      is_audible_ = web_contents->IsCurrentlyAudible();
      is_foreground_ = IsForeground(web_contents->GetVisibility());
    }
  }
  ~PinnedTabObserver() override { ClearObservation(); }

  PinnedTabObserver(const PinnedTabObserver&) = delete;
  PinnedTabObserver& operator=(const PinnedTabObserver&) = delete;

  // content::WebContentsObserver.
  void OnAudioStateChanged(bool audible) override {
    bool was_observable = IsObservable();
    is_audible_ = audible;
    if (was_observable != IsObservable()) {
      UpdateTabDataAndSend(CreateTabData(web_contents()));
    }
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    bool was_observable = IsObservable();
    is_foreground_ = IsForeground(visibility);
    if (was_observable != IsObservable()) {
      UpdateTabDataAndSend(CreateTabData(web_contents()));
    }
  }

  // tabs::TabInterface
  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason) {
    if (reason == tabs::TabInterface::DetachReason::kDelete) {
      ClearObservation();
      std::move(tab_will_close_).Run(tab->GetHandle());
    }
  }

  void OnWillDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents) {
    CHECK_EQ(web_contents(), old_contents);
    StartObservation(new_contents);
  }

  void FocusedTabDataChanged(glic::mojom::TabDataPtr tab_data) {
    UpdateTabDataAndSend(std::move(tab_data));
  }

  bool IsObservable() const { return is_foreground_ || is_audible_; }

 private:
  void UpdateTabDataAndSend(glic::mojom::TabDataPtr tab_data) {
    // Add observability info.
    tab_data->is_observable = IsObservable();
    tab_data_changed_.Run(tab_->GetHandle(), std::move(tab_data));
  }

  void StartObservation(content::WebContents* contents) {
    Observe(contents);
    tab_data_observer_ = std::make_unique<TabDataObserver>(
        contents, base::BindRepeating(&PinnedTabObserver::FocusedTabDataChanged,
                                      base::Unretained(this)));
  }

  void ClearObservation() {
    Observe(nullptr);
    tab_data_observer_.reset();
  }

  raw_ptr<tabs::TabInterface> tab_;

  base::CallbackListSubscription will_discard_contents_subscription_;
  base::CallbackListSubscription will_detach_subscription_;

  bool is_foreground_ = false;
  bool is_audible_ = false;

  std::unique_ptr<TabDataObserver> tab_data_observer_;
  base::RepeatingCallback<void(tabs::TabHandle, glic::mojom::TabDataPtr)>
      tab_data_changed_;

  base::OnceCallback<void(tabs::TabHandle)> tab_will_close_;
};

GlicPinnedTabManager::PinnedTabEntry::PinnedTabEntry(
    tabs::TabHandle tab_handle,
    std::unique_ptr<PinnedTabObserver> tab_observer)
    : tab_handle(tab_handle), tab_observer(std::move(tab_observer)) {}

GlicPinnedTabManager::PinnedTabEntry::~PinnedTabEntry() = default;

GlicPinnedTabManager::PinnedTabEntry::PinnedTabEntry(PinnedTabEntry&& other) {
  *this = std::move(other);
}

GlicPinnedTabManager::PinnedTabEntry&
GlicPinnedTabManager::PinnedTabEntry::operator=(PinnedTabEntry&& other) {
  tab_handle = std::move(other.tab_handle);
  tab_observer = std::move(other.tab_observer);
  return *this;
}

GlicPinnedTabManager::GlicPinnedTabManager(
    GlicSharingManagerImpl* sharing_manager)
    : sharing_manager_(sharing_manager),
      max_pinned_tabs_(kDefaultMaxPinnedTabs) {}

GlicPinnedTabManager::~GlicPinnedTabManager() = default;

base::CallbackListSubscription
GlicPinnedTabManager::AddPinnedTabsChangedCallback(
    PinnedTabsChangedCallback callback) {
  return pinned_tabs_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicPinnedTabManager::AddPinnedTabDataChangedCallback(
    PinnedTabDataChangedCallback callback) {
  return pinned_tab_data_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicPinnedTabManager::AddTabPinningStatusChangedCallback(
    TabPinningStatusChangedCallback callback) {
  return pinning_status_changed_callback_list_.Add(std::move(callback));
}

bool GlicPinnedTabManager::PinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  bool pinning_fully_succeeded = true;
  for (const auto tab_handle : tab_handles) {
    if (pinned_tabs_.size() == static_cast<size_t>(max_pinned_tabs_)) {
      pinning_fully_succeeded = false;
      break;
    }
    auto* tab = tab_handle.Get();
    if (!tab || IsTabPinned(tab_handle) ||
        !sharing_manager_->IsBrowserValidForSharing(
            tab->GetBrowserWindowInterface())) {
      pinning_fully_succeeded = false;
      continue;
    }
    pinned_tabs_.emplace_back(
        tab_handle,
        std::make_unique<PinnedTabObserver>(
            tab_handle.Get(),
            base::BindRepeating(&GlicPinnedTabManager::OnTabDataChanged,
                                weak_ptr_factory_.GetWeakPtr()),
            base::BindOnce(&GlicPinnedTabManager::OnTabWillClose,
                           weak_ptr_factory_.GetWeakPtr())));
    pinning_status_changed_callback_list_.Notify(tab_handle.Get(), true);
  }
  NotifyPinnedTabsChanged();
  return pinning_fully_succeeded;
}

bool GlicPinnedTabManager::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  bool unpinning_fully_succeeded = true;
  for (const auto tab_handle : tab_handles) {
    auto* tab = tab_handle.Get();
    if (!tab || !IsTabPinned(tab_handle)) {
      unpinning_fully_succeeded = false;
      continue;
    }
    std::erase_if(pinned_tabs_, [tab_handle](const PinnedTabEntry& entry) {
      return entry.tab_handle == tab_handle;
    });
    pinning_status_changed_callback_list_.Notify(tab_handle.Get(), true);
  }
  NotifyPinnedTabsChanged();
  return unpinning_fully_succeeded;
}

void GlicPinnedTabManager::UnpinAllTabs() {
  std::vector<tabs::TabHandle> tabs_to_unpin;
  for (auto& entry : pinned_tabs_) {
    tabs_to_unpin.push_back(entry.tab_handle);
  }
  UnpinTabs(tabs_to_unpin);
}

const GlicPinnedTabManager::PinnedTabEntry*
GlicPinnedTabManager::GetPinnedTabEntry(tabs::TabHandle tab_handle) const {
  auto it = std::find_if(pinned_tabs_.begin(), pinned_tabs_.end(),
                         [tab_handle](const PinnedTabEntry& entry) {
                           return entry.tab_handle == tab_handle;
                         });
  if (it == pinned_tabs_.end()) {
    return nullptr;
  }
  return &(*it);
}

uint32_t GlicPinnedTabManager::SetMaxPinnedTabs(uint32_t max_pinned_tabs) {
  if (max_pinned_tabs < GetNumPinnedTabs()) {
    max_pinned_tabs = GetNumPinnedTabs();
  } else if (max_pinned_tabs > kMaxPinnedTabs) {
    max_pinned_tabs = kMaxPinnedTabs;
  }
  max_pinned_tabs_ = max_pinned_tabs;
  return max_pinned_tabs;
}

uint32_t GlicPinnedTabManager::GetMaxPinnedTabs() const {
  return max_pinned_tabs_;
}

uint32_t GlicPinnedTabManager::GetNumPinnedTabs() const {
  return static_cast<uint32_t>(pinned_tabs_.size());
}

bool GlicPinnedTabManager::IsTabPinned(tabs::TabHandle tab_handle) const {
  return !!GetPinnedTabEntry(tab_handle);
}

std::vector<content::WebContents*> GlicPinnedTabManager::GetPinnedTabs() const {
  std::vector<content::WebContents*> pinned_contents;
  for (auto& entry : pinned_tabs_) {
    pinned_contents.push_back(entry.tab_observer->web_contents());
  }
  return pinned_contents;
}

void GlicPinnedTabManager::NotifyPinnedTabsChanged() {
  pinned_tabs_changed_callback_list_.Notify(GetPinnedTabs());
}

void GlicPinnedTabManager::OnTabDataChanged(tabs::TabHandle tab_handle,
                                            glic::mojom::TabDataPtr tab_data) {
  CHECK(IsTabPinned(tab_handle));
  pinned_tab_data_changed_callback_list_.Notify(tab_data ? tab_data.get()
                                                         : nullptr);
}

void GlicPinnedTabManager::OnTabWillClose(tabs::TabHandle tab_handle) {
  // TODO(b/426644733): Avoid n^2 work when closing all tabs.
  CHECK(UnpinTabs({tab_handle}));
  NotifyPinnedTabsChanged();
}

}  // namespace glic
