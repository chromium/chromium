// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_RESTORE_DATA_COLLECTOR_H_
#define ASH_WM_DESKS_TEMPLATES_RESTORE_DATA_COLLECTOR_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/account_id/account_id.h"
#include "ui/aura/window_tracker.h"

namespace app_restore {
class RestoreData;
struct AppLaunchInfo;
struct WindowInfo;
}  // namespace app_restore

namespace aura {
class Window;
}

namespace ash {

class DeskTemplate;
enum class DeskTemplateType;

// Collects `AppLaunchData` from all applications that are currently active, and
// returns it in form of `DeskTemplate` record.
class RestoreDataCollector {
 public:
  using GetDeskTemplateCallback =
      base::OnceCallback<void(std::unique_ptr<DeskTemplate>)>;

  RestoreDataCollector();
  RestoreDataCollector(const RestoreDataCollector&) = delete;
  RestoreDataCollector& operator=(const RestoreDataCollector&) = delete;
  ~RestoreDataCollector();

  // Captures the active desk and returns it as a `DeskTemplate` object via the
  // `callback`.
  void CaptureActiveDeskAsSavedDesk(GetDeskTemplateCallback callback,
                                    DeskTemplateType template_type,
                                    const std::string& template_name,
                                    aura::Window* root_window_to_show,
                                    AccountId current_account_id);

 private:
  // Keeps the state for the asynchronous call for `AppLaunchData` to the apps.
  struct Call {
    Call();
    Call(Call&&);
    Call& operator=(Call&&);
    ~Call();

    DeskTemplateType template_type;
    std::string template_name;
    raw_ptr<aura::Window> root_window_to_show;
    std::vector<raw_ptr<aura::Window, VectorExperimental>> unsupported_apps;
    size_t non_persistable_window_count = 0;
    std::unique_ptr<app_restore::RestoreData> data;
    uint32_t pending_request_count = 0;
    uint64_t lacros_profile_id = 0;
    GetDeskTemplateCallback callback;
  };

  // Receives the `AppLaunchInfo` for the single app and puts it into the
  // RestoreData record where data from all clients is accumulated.  If all data
  // is collected, invokes the `SendDeskTemplate()` method.
  void OnAppLaunchDataReceived(
      uint32_t serial,
      const std::string& app_id,
      std::unique_ptr<app_restore::WindowInfo> window_info,
      std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info);

  // Creates a `DeskTemplate` object and sends it to the consumer after all apps
  // have delivered their `AppLaunchInfo`.
  void SendDeskTemplate(uint32_t serial);

  // Auxiliary data for maintaining the asynchronous polling of the windows.
  uint32_t serial_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::flat_map<uint32_t, Call> calls_ GUARDED_BY_CONTEXT(sequence_checker_);
  aura::WindowTracker window_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RestoreDataCollector> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_RESTORE_DATA_COLLECTOR_H_
