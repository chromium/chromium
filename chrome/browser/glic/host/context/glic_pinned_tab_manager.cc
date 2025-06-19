// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"

#include "chrome/browser/glic/host/context/glic_tab_data.h"

namespace glic {

namespace {

// An arbitrary limit.
const int32_t kDefaultMaxPinnedTabs = 5;

}  // namespace

class GlicPinnedTabManager::PinnedTabObserver {
 public:
  PinnedTabObserver(tabs::TabInterface* tab,
                    base::OnceCallback<void(tabs::TabHandle)> tab_will_close)
      : tab_will_close_(std::move(tab_will_close)) {
    will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
        &PinnedTabObserver::OnWillDetach, base::Unretained(this)));
  }
  ~PinnedTabObserver() = default;

  PinnedTabObserver(const PinnedTabObserver&) = delete;
  PinnedTabObserver& operator=(const PinnedTabObserver&) = delete;

  // tabs::TabInterface
  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason) {
    if (reason == tabs::TabInterface::DetachReason::kDelete) {
      std::move(tab_will_close_).Run(tab->GetHandle());
    }
  }

 private:
  base::CallbackListSubscription will_detach_subscription_;
  base::OnceCallback<void(tabs::TabHandle)> tab_will_close_;
};

GlicPinnedTabManager::PinnedTabEntry::PinnedTabEntry(
    tabs::TabHandle tab_handle,
    std::unique_ptr<PinnedTabObserver> tab_observer)
    : tab_handle(tab_handle), tab_observer(std::move(tab_observer)) {}

GlicPinnedTabManager::PinnedTabEntry::PinnedTabEntry(PinnedTabEntry&& other) {
  *this = std::move(other);
}

GlicPinnedTabManager::PinnedTabEntry&
GlicPinnedTabManager::PinnedTabEntry::operator=(PinnedTabEntry&& other) {
  tab_handle = std::move(other.tab_handle);
  tab_observer = std::move(other.tab_observer);
  return *this;
}

GlicPinnedTabManager::PinnedTabEntry::~PinnedTabEntry() = default;

GlicPinnedTabManager::GlicPinnedTabManager()
    : max_pinned_tabs_(kDefaultMaxPinnedTabs) {}
GlicPinnedTabManager::~GlicPinnedTabManager() = default;

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
    if (!tab || IsTabPinned(tab_handle)) {
      pinning_fully_succeeded = false;
      continue;
    }
    pinned_tabs_.emplace_back(
        tab_handle, std::make_unique<PinnedTabObserver>(
                        tab_handle.Get(),
                        base::BindOnce(&GlicPinnedTabManager::OnTabWillClose,
                                       base::Unretained(this))));
    pinning_status_changed_callback_list_.Notify(tab_handle.Get(), true);
  }
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
  return unpinning_fully_succeeded;
}

void GlicPinnedTabManager::UnpinAllTabs() {
  std::vector<tabs::TabHandle> tabs_to_unpin;
  for (auto& entry : pinned_tabs_) {
    tabs_to_unpin.push_back(entry.tab_handle);
  }
  UnpinTabs(tabs_to_unpin);
}

int32_t GlicPinnedTabManager::GetMaxPinnedTabs() const {
  return max_pinned_tabs_;
}

int32_t GlicPinnedTabManager::GetNumPinnedTabs() const {
  return static_cast<int32_t>(pinned_tabs_.size());
}

bool GlicPinnedTabManager::IsTabPinned(tabs::TabHandle tab_handle) const {
  return std::find_if(pinned_tabs_.begin(), pinned_tabs_.end(),
                      [tab_handle](const PinnedTabEntry& entry) {
                        return entry.tab_handle == tab_handle;
                      }) != pinned_tabs_.end();
}

void GlicPinnedTabManager::OnTabWillClose(tabs::TabHandle tab_handle) {
  CHECK(UnpinTabs({tab_handle}));
}

}  // namespace glic
