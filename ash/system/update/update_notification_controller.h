// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UPDATE_UPDATE_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_UPDATE_UPDATE_NOTIFICATION_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/model/update_model.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"

namespace gfx {
struct VectorIcon;
}
namespace message_center {
enum class SystemNotificationWarningLevel;
}

namespace ash {

class ShutdownConfirmationDialog;

// Controller class to manage update notification.
class ASH_EXPORT UpdateNotificationController : public UpdateObserver {
 public:
  UpdateNotificationController();

  UpdateNotificationController(const UpdateNotificationController&) = delete;
  UpdateNotificationController& operator=(const UpdateNotificationController&) =
      delete;

  ~UpdateNotificationController() override;

  // UpdateObserver:
  void OnUpdateAvailable() override;

  // Callback functions for Shutdown Confirmation Dialog which is generated
  // when the device bootup process is occasionally slow - eg. memory training
  // during the bootup due to a system firmware update.
  void RestartForUpdate();
  void RestartCancelled();

 private:
  friend class UpdateNotificationControllerTest;

  bool ShouldShowUpdate() const;
  bool ShouldShowDeferredUpdate() const;
  std::u16string GetTitle() const;
  std::u16string GetMessage() const;
  const gfx::VectorIcon& GetIcon() const;
  message_center::SystemNotificationWarningLevel GetWarningLevel() const;
  void HandleNotificationClick(std::optional<int> index);
  void GenerateUpdateNotification(
      std::optional<bool> slow_boot_file_path_exists);

  const raw_ptr<UpdateModel> model_;

  base::FilePath slow_boot_file_path_;
  bool slow_boot_file_path_exists_ = false;
  raw_ptr<ShutdownConfirmationDialog> confirmation_dialog_ = nullptr;

  base::WeakPtrFactory<UpdateNotificationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UPDATE_UPDATE_NOTIFICATION_CONTROLLER_H_
