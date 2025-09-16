// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_TAB_MANAGER_H_

#include "chrome/browser/glic/host/context/glic_focused_tab_manager_interface.h"

namespace glic {

class GlicEmptyFocusedTabManager : public GlicFocusedTabManagerInterface {
 public:
  GlicEmptyFocusedTabManager();
  ~GlicEmptyFocusedTabManager() override;

  GlicEmptyFocusedTabManager(const GlicEmptyFocusedTabManager&) = delete;
  GlicEmptyFocusedTabManager& operator=(const GlicEmptyFocusedTabManager&) =
      delete;

  // GlicFocusedTabManagerInterface Implementation.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const FocusedTabData&)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback) override;
  FocusedTabData GetFocusedTabData() override;
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback) override;
  bool IsTabFocused(tabs::TabHandle tab_handle) const override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_TAB_MANAGER_H_
