// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_INTERFACE_H_

#include "base/callback_list.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"

namespace glic {

// Responsible for managing which tab is considered "focused" and for accessing
// its WebContents.
class GlicFocusedTabManagerInterface {
 public:
  GlicFocusedTabManagerInterface() = default;
  virtual ~GlicFocusedTabManagerInterface() = default;
  GlicFocusedTabManagerInterface(const GlicFocusedTabManagerInterface&) =
      delete;
  GlicFocusedTabManagerInterface& operator=(
      const GlicFocusedTabManagerInterface&) = delete;

  // Callback for changes to focused tab. If no tab is in focus an error reason
  // is returned indicating why and maybe a tab candidate with details as to
  // why it cannot be focused.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  virtual base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) = 0;

  // Callback for changes to the tab data representation of the focused tab.
  // This includes any event that changes tab data -- e.g. favicon/title change
  // events (where the container does not change), as well as container changed
  // events.
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  virtual base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) = 0;

  virtual bool IsTabFocused(tabs::TabHandle tab_handle) const = 0;

  // Returns the currently focused tab data or an error reason stating why one
  // was not available. This may also contain a tab candidate along with details
  // as to why it cannot be focused. Virtual for testing.
  virtual FocusedTabData GetFocusedTabData() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_TAB_MANAGER_INTERFACE_H_
