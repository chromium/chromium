// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_H_

#include "base/containers/queue.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "ui/base/ui_base_types.h"

namespace crosapi {

struct VersionedBrowserService {
  mojom::BrowserService* service;
  uint32_t interface_version;
};

// Base class representing the browser actions that BrowserManager provides.
class BrowserAction {
 public:
  explicit BrowserAction(bool is_queueable) : is_queueable_(is_queueable) {}
  virtual ~BrowserAction() = default;

  using NewFullscreenWindowCallback =
      base::OnceCallback<void(crosapi::mojom::CreationResult)>;
  using NewWindowForDetachingTabCallback =
      base::OnceCallback<void(crosapi::mojom::CreationResult,
                              const std::string&)>;

  // Factory functions for creating specific browser actions. See
  // the browser_manager.h for documentation.
  static std::unique_ptr<BrowserAction> NewWindow(
      bool incognito,
      bool should_trigger_session_restore,
      int64_t target_display_id);
  static std::unique_ptr<BrowserAction> NewTab();
  static std::unique_ptr<BrowserAction> Launch(int64_t target_display_id);
  static std::unique_ptr<BrowserAction> NewWindowForDetachingTab(
      base::StringPiece16 tab_id_str,
      base::StringPiece16 group_id_str,
      NewWindowForDetachingTabCallback callback);
  static std::unique_ptr<BrowserAction> NewGuestWindow(int64_t target_display);
  static std::unique_ptr<BrowserAction> NewFullscreenWindow(
      const GURL& url,
      int64_t target_display,
      NewFullscreenWindowCallback callback);
  static std::unique_ptr<BrowserAction> OpenUrl(
      const GURL& url,
      crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
      crosapi::mojom::OpenUrlFrom from,
      NavigateParams::PathBehavior path_behavior);
  static std::unique_ptr<BrowserAction> OpenForFullRestore(
      bool skip_crash_restore);
  static std::unique_ptr<BrowserAction> RestoreTab();
  static std::unique_ptr<BrowserAction> HandleTabScrubbing(float x_offset);
  static std::unique_ptr<BrowserAction> CreateBrowserWithRestoredData(
      const std::vector<GURL>& urls,
      const gfx::Rect& bounds,
      ui::WindowShowState show_state,
      int32_t active_tab_index,
      base::StringPiece app_name,
      int32_t restore_window_id);

  // Returns the initial action for the automatic start of Lacros.
  // No window will be opened in the following circumstances:
  // 1. Lacros-chrome is initialized in the web Kiosk session
  // 2. Full restore is responsible for restoring/launching Lacros.
  static std::unique_ptr<BrowserAction> GetActionForSessionStart();

  // Performs the action.
  virtual void Perform(const VersionedBrowserService& service) = 0;

  // Cancels the action.
  virtual void Cancel(crosapi::mojom::CreationResult reason) {}

  // Returns whether the action can be be postponed when Lacros isn't ready.
  bool IsQueueable() const { return is_queueable_; }

 private:
  const bool is_queueable_;
};

// A queue of queueable actions.
class BrowserActionQueue {
 public:
  BrowserActionQueue();
  ~BrowserActionQueue();

  // Enqueues |action| if it is queueable. Cancels it otherwise.
  void PushOrCancel(std::unique_ptr<BrowserAction> action);

  void Push(std::unique_ptr<BrowserAction> action);
  std::unique_ptr<BrowserAction> Pop();
  bool IsEmpty() const;
  void Clear();

 private:
  base::queue<std::unique_ptr<BrowserAction>> actions_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_ACTION_H_
