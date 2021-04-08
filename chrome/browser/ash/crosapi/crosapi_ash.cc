// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/crosapi_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_ash.h"
#include "ash/components/account_manager/account_manager_factory.h"
#include "chrome/browser/ash/crosapi/automation_ash.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/cert_database_ash.h"
#include "chrome/browser/ash/crosapi/clipboard_ash.h"
#include "chrome/browser/ash/crosapi/device_attributes_ash.h"
#include "chrome/browser/ash/crosapi/feedback_ash.h"
#include "chrome/browser/ash/crosapi/file_manager_ash.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/message_center_ash.h"
#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"
#include "chrome/browser/ash/crosapi/prefs_ash.h"
#include "chrome/browser/ash/crosapi/screen_manager_ash.h"
#include "chrome/browser/ash/crosapi/select_file_ash.h"
#include "chrome/browser/ash/crosapi/task_manager_ash.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "chrome/browser/ash/crosapi/url_handler_ash.h"
#include "chrome/browser/ash/crosapi/video_capture_device_factory_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/file_manager.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"

namespace crosapi {

CrosapiAsh::CrosapiAsh()
    : automation_ash_(std::make_unique<AutomationAsh>()),
      browser_service_host_ash_(std::make_unique<BrowserServiceHostAsh>()),
      cert_database_ash_(std::make_unique<CertDatabaseAsh>()),
      clipboard_ash_(std::make_unique<ClipboardAsh>()),
      device_attributes_ash_(std::make_unique<DeviceAttributesAsh>()),
      feedback_ash_(std::make_unique<FeedbackAsh>()),
      file_manager_ash_(std::make_unique<FileManagerAsh>()),
      idle_service_ash_(std::make_unique<IdleServiceAsh>()),
      keystore_service_ash_(std::make_unique<KeystoreServiceAsh>()),
      message_center_ash_(std::make_unique<MessageCenterAsh>()),
      metrics_reporting_ash_(std::make_unique<MetricsReportingAsh>(
          g_browser_process->local_state())),
      prefs_ash_(
          std::make_unique<PrefsAsh>(g_browser_process->profile_manager(),
                                     g_browser_process->local_state())),
      screen_manager_ash_(std::make_unique<ScreenManagerAsh>()),
      select_file_ash_(std::make_unique<SelectFileAsh>()),
      task_manager_ash_(std::make_unique<TaskManagerAsh>()),
      test_controller_ash_(std::make_unique<TestControllerAsh>()),
      url_handler_ash_(std::make_unique<UrlHandlerAsh>()),
      video_capture_device_factory_ash_(
          std::make_unique<VideoCaptureDeviceFactoryAsh>()) {
  receiver_set_.set_disconnect_handler(base::BindRepeating(
      &CrosapiAsh::OnDisconnected, weak_factory_.GetWeakPtr()));
}

CrosapiAsh::~CrosapiAsh() {
  // Invoke all disconnect handlers.
  auto handlers = std::move(disconnect_handler_map_);
  for (auto& entry : handlers)
    std::move(entry.second).Run();
}

void CrosapiAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
    CrosapiId crosapi_id,
    base::OnceClosure disconnect_handler) {
  mojo::ReceiverId id =
      receiver_set_.Add(this, std::move(pending_receiver), crosapi_id);
  if (!disconnect_handler.is_null())
    disconnect_handler_map_.emplace(id, std::move(disconnect_handler));
}

void CrosapiAsh::BindAutomationDeprecated(
    mojo::PendingReceiver<mojom::Automation> receiver) {}

void CrosapiAsh::BindAutomationFactory(
    mojo::PendingReceiver<mojom::AutomationFactory> receiver) {
  automation_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindAccountManager(
    mojo::PendingReceiver<mojom::AccountManager> receiver) {
  // Assumptions:
  // 1. TODO(https://crbug.com/1102768): Multi-Signin / Fast-User-Switching is
  // disabled.
  // 2. ash-chrome has 1 and only 1 "regular" |Profile|.
#if DCHECK_IS_ON()
  int num_regular_profiles = 0;
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (chromeos::ProfileHelper::IsRegularProfile(profile))
      num_regular_profiles++;
  }
  DCHECK_EQ(1, num_regular_profiles);
#endif  // DCHECK_IS_ON()
  // Given these assumptions, there is 1 and only 1 AccountManagerAsh that
  // can/should be contacted - the one attached to the regular |Profile| in
  // ash-chrome, for the current |User|.
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  const Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  crosapi::AccountManagerAsh* const account_manager_ash =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManagerAsh(
              /* profile_path = */ profile->GetPath().value());
  account_manager_ash->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindBrowserServiceHost(
    mojo::PendingReceiver<crosapi::mojom::BrowserServiceHost> receiver) {
  browser_service_host_ash_->BindReceiver(receiver_set_.current_context(),
                                          std::move(receiver));
}

void CrosapiAsh::BindFileManager(
    mojo::PendingReceiver<crosapi::mojom::FileManager> receiver) {
  file_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindIdleService(
    mojo::PendingReceiver<crosapi::mojom::IdleService> receiver) {
  idle_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindKeystoreService(
    mojo::PendingReceiver<crosapi::mojom::KeystoreService> receiver) {
  keystore_service_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMessageCenter(
    mojo::PendingReceiver<mojom::MessageCenter> receiver) {
  message_center_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMetricsReporting(
    mojo::PendingReceiver<mojom::MetricsReporting> receiver) {
  metrics_reporting_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSelectFile(
    mojo::PendingReceiver<mojom::SelectFile> receiver) {
  select_file_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindScreenManager(
    mojo::PendingReceiver<mojom::ScreenManager> receiver) {
  screen_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindHidManager(
    mojo::PendingReceiver<device::mojom::HidManager> receiver) {
  content::GetDeviceService().BindHidManager(std::move(receiver));
}

void CrosapiAsh::BindFeedback(mojo::PendingReceiver<mojom::Feedback> receiver) {
  feedback_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMediaSessionController(
    mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
        receiver) {
  content::GetMediaSessionService().BindMediaControllerManager(
      std::move(receiver));
}

void CrosapiAsh::BindMediaSessionAudioFocus(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver) {
  content::GetMediaSessionService().BindAudioFocusManager(std::move(receiver));
}

void CrosapiAsh::BindMediaSessionAudioFocusDebug(
    mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
        receiver) {
  content::GetMediaSessionService().BindAudioFocusManagerDebug(
      std::move(receiver));
}

void CrosapiAsh::BindCertDatabase(
    mojo::PendingReceiver<mojom::CertDatabase> receiver) {
  cert_database_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTaskManager(
    mojo::PendingReceiver<mojom::TaskManager> receiver) {
  task_manager_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindTestController(
    mojo::PendingReceiver<mojom::TestController> receiver) {
  test_controller_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindClipboard(
    mojo::PendingReceiver<mojom::Clipboard> receiver) {
  clipboard_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindDeviceAttributes(
    mojo::PendingReceiver<mojom::DeviceAttributes> receiver) {
  device_attributes_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterClient(
      std::move(remote));
}

void CrosapiAsh::BindPrefs(mojo::PendingReceiver<mojom::Prefs> receiver) {
  prefs_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindUrlHandler(
    mojo::PendingReceiver<mojom::UrlHandler> receiver) {
  url_handler_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->BindMachineLearningService(std::move(receiver));
}

void CrosapiAsh::BindVideoCaptureDeviceFactory(
    mojo::PendingReceiver<mojom::VideoCaptureDeviceFactory> receiver) {
  video_capture_device_factory_ash_->BindReceiver(std::move(receiver));
}

void CrosapiAsh::OnBrowserStartup(mojom::BrowserInfoPtr browser_info) {
  BrowserManager::Get()->set_browser_version(browser_info->browser_version);
}

void CrosapiAsh::OnDisconnected() {
  auto it = disconnect_handler_map_.find(receiver_set_.current_receiver());
  if (it == disconnect_handler_map_.end())
    return;

  base::OnceClosure callback = std::move(it->second);
  disconnect_handler_map_.erase(it);
  std::move(callback).Run();
}

}  // namespace crosapi
