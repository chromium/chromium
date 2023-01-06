// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_VERSION_UPDATER_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_VERSION_UPDATER_H_

#include <string>

#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine.pb.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

namespace ash {
namespace shimless_rma {

// TODO(gavindodd): Consider if this can be shared with
// chrome/browser/ash/login/version_updater/version_updater

class VersionUpdater : public UpdateEngineClient::Observer {
 public:
  typedef base::RepeatingCallback<void(update_engine::Operation operation,
                                       double progress,
                                       bool rollback,
                                       bool powerwash,
                                       const std::string& version,
                                       int64_t update_size,
                                       update_engine::ErrorCode error_code)>
      OsUpdateStatusCallback;

  VersionUpdater();
  VersionUpdater(const VersionUpdater&) = delete;
  VersionUpdater& operator=(const VersionUpdater&) = delete;

  ~VersionUpdater() override;

  void SetOsUpdateStatusCallback(OsUpdateStatusCallback callback);
  void CheckOsUpdateAvailable();
  bool UpdateOs();
  bool IsUpdateEngineIdle();

  void UpdateStatusChangedForTesting(const update_engine::StatusResult& status);
  void DisableUpdateOnceForTesting();

 private:
  // Callback from UpdateEngineClient::RequestUpdateCheck().
  void OnRequestUpdateCheck(UpdateEngineClient::UpdateCheckResult result);

  // UpdateEngineClient::Observer implementation.
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  OsUpdateStatusCallback status_callback_;
  enum CheckUpdateState {
    IDLE,
    CHECKING,
    UPDATE_AVAILABLE,
    NO_UPDATE_AVAILABLE
  };
  CheckUpdateState check_update_available_ = IDLE;
  std::string new_version_;
  bool disable_update_for_testing_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<VersionUpdater> weak_ptr_factory_{this};
};

}  // namespace shimless_rma
}  // namespace ash

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_VERSION_UPDATER_H_
