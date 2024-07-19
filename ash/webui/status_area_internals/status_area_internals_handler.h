// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_HANDLER_H_
#define ASH_WEBUI_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_HANDLER_H_

#include "ash/webui/status_area_internals/mojom/status_area_internals.mojom.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class ScopedFakeSystemTrayModel;
class ScopedFakePowerStatus;

// WebUI message handler for chrome://status-area-internals from the Chrome page
// to the System UI.
class StatusAreaInternalsHandler
    : public mojom::status_area_internals::PageHandler {
 public:
  explicit StatusAreaInternalsHandler(
      mojo::PendingReceiver<mojom::status_area_internals::PageHandler>
          receiver);
  StatusAreaInternalsHandler(const StatusAreaInternalsHandler&) = delete;
  StatusAreaInternalsHandler& operator=(const StatusAreaInternalsHandler&) =
      delete;
  ~StatusAreaInternalsHandler() override;

  // mojom::status_area_internals::PageHandler:
  void ToggleImeTray(bool visible) override;
  void TogglePaletteTray(bool visible) override;
  void ToggleLogoutTray(bool visible) override;
  void ToggleVirtualKeyboardTray(bool visible) override;
  void ToggleDictationTray(bool visible) override;
  void ToggleVideoConferenceTray(bool visible) override;
  void ToggleAnnotationTray(bool visible) override;
  void SetIsInUserChildSession(bool in_child_session) override;
  void TriggerPrivacyIndicators(const std::string& app_id,
                                const std::string& app_name,
                                bool is_camera_used,
                                bool is_microphone_used) override;
  void ResetHmrConsentStatus() override;
  void SetBatteryIcon(const PageHandler::BatteryIcon icon) override;
  void SetBatteryPercent(double percent) override;

 private:
  friend class StatusAreaInternalsHandlerTest;
  friend class StatusAreaInternalsHandlerBatteryTest;
  mojo::Receiver<mojom::status_area_internals::PageHandler> receiver_;

  std::unique_ptr<ScopedFakeSystemTrayModel> scoped_fake_model_;

  std::unique_ptr<ScopedFakePowerStatus> scoped_fake_power_status_;

  base::WeakPtrFactory<StatusAreaInternalsHandler> weak_pointer_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_HANDLER_H_
