// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_H_

#include "base/containers/span.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

// Responsible for managing all shared context (focused tabs, explicitly-shared
// tabs).
class GlicSharingManager {
 public:
  GlicSharingManager() = default;
  virtual ~GlicSharingManager() = default;
  GlicSharingManager(const GlicSharingManager&) = delete;
  GlicSharingManager& operator=(const GlicSharingManager&) = delete;

  // Callback for changes to focused tab. If no tab is in focus an error reason
  // is returned indicating why and maybe a tab candidate with details as to
  // why it cannot be focused.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  virtual base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) = 0;

  // Returns the currently focused tab data or an error reason stating why one
  // was not available. This may also contain a tab candidate along with details
  // as to why it cannot be focused. Virtual for testing.
  virtual FocusedTabData GetFocusedTabData() = 0;

  // Registers a callback to be invoked when the pinned status of a tab changes.
  using TabPinningStatusChangedCallback =
      base::RepeatingCallback<void(tabs::TabInterface*, bool)>;
  virtual base::CallbackListSubscription AddTabPinningStatusChangedCallback(
      TabPinningStatusChangedCallback callback) = 0;

  // Pins the specified tabs. If we are only able to pin `n` tabs within the
  // limit, the first `n` tabs from this collection will be pinned and we will
  // return false (to indicate that it was not fully successful). If any of the
  // tab handles correspond to a tab that either doesn't exist or is already
  // pinned, it will be skipped and we will similarly return false to indicate
  // that the function was not fully successful.
  virtual bool PinTabs(base::span<const tabs::TabHandle> tab_handles) = 0;

  // Unpins the specified tabs. If any of the tab handles correspond to a tab
  // that either doesn't exist or is not pinned, it will be skipped and we will
  // similarly return false to indicate that the function was not fully
  // successful.
  virtual bool UnpinTabs(base::span<const tabs::TabHandle> tab_handles) = 0;

  // Unpins all pinned tabs, if any.
  virtual void UnpinAllTabs() = 0;

  // Gets the limit on the number of pinned tabs.
  virtual int32_t GetMaxPinnedTabs() const = 0;

  // Gets the current number of pinned tabs.
  virtual int32_t GetNumPinnedTabs() const = 0;

  // Queries whether the given tab has been explicitly pinned.
  virtual bool IsTabPinned(tabs::TabHandle tab_handle) const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_H_
