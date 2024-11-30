// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/containers/queue.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chromeos/crosapi/mojom/browser_service.mojom.h"
#include "components/tab_groups/tab_group_info.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"

namespace crosapi {

struct VersionedBrowserService {
  raw_ptr<mojom::BrowserService> service;
  uint32_t interface_version;
};

// Base class representing the browser actions that BrowserManager provides.
class BrowserAction {
 public:
  explicit BrowserAction(bool is_queueable) : is_queueable_(is_queueable) {}
  virtual ~BrowserAction() = default;

  static std::unique_ptr<BrowserAction> OpenUrl(
      const GURL& url,
      crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
      crosapi::mojom::OpenUrlFrom from,
      NavigateParams::PathBehavior path_behavior);

  // The type of BrowserManager::OnActionPerformed.
  using BrowserManagerCallback = base::OnceCallback<void(bool)>;

  // Performs the action.
  // The provided callback should be called to signal completion or request a
  // retry at a later point. The callback actually owns the action and
  // signalling completion generally results in destroying the action, so care
  // needs to be taken not to access anymore afterwards.
  virtual void Perform(const VersionedBrowserService& service,
                       BrowserManagerCallback on_performed) = 0;

  // Cancels the action.
  virtual void Cancel(crosapi::mojom::CreationResult reason);

  // Returns whether the action can be be postponed when Lacros isn't ready.
  bool IsQueueable() const { return is_queueable_; }

 protected:
  // Run on_performed, requesting a retry depending on the given result. This is
  // meant to be used as mojo callback by action subclasses whose response does
  // not need additional arguments.
  void OnPerformed(BrowserManagerCallback on_performed,
                   mojom::CreationResult result);

 private:
  const bool is_queueable_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_H_
