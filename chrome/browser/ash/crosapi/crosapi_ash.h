// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/crosapi_id.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

class AutomationAsh;
class BrowserServiceHostAsh;
class CertDatabaseAsh;
class ClipboardAsh;
class DeviceAttributesAsh;
class FeedbackAsh;
class FileManagerAsh;
class IdleServiceAsh;
class KeystoreServiceAsh;
class MessageCenterAsh;
class MetricsReportingAsh;
class PrefsAsh;
class ScreenManagerAsh;
class SelectFileAsh;
class TaskManagerAsh;
class TestControllerAsh;
class UrlHandlerAsh;
class VideoCaptureDeviceFactoryAsh;

// Implementation of Crosapi in Ash. It provides a set of APIs that
// crosapi clients, such as lacros-chrome, can call into.
class CrosapiAsh : public mojom::Crosapi {
 public:
  CrosapiAsh();
  ~CrosapiAsh() override;

  // Binds the given receiver to this instance.
  // |disconnected_handler| is called on the connection lost.
  void BindReceiver(mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
                    CrosapiId crosapi_id,
                    base::OnceClosure disconnect_handler);

  // crosapi::mojom::Crosapi:
  void BindAutomationDeprecated(
      mojo::PendingReceiver<mojom::Automation> receiver) override;
  void BindAutomationFactory(
      mojo::PendingReceiver<mojom::AutomationFactory> receiver) override;
  void BindAccountManager(
      mojo::PendingReceiver<mojom::AccountManager> receiver) override;
  void BindBrowserServiceHost(
      mojo::PendingReceiver<mojom::BrowserServiceHost> receiver) override;
  void BindCertDatabase(
      mojo::PendingReceiver<mojom::CertDatabase> receiver) override;
  void BindClipboard(mojo::PendingReceiver<mojom::Clipboard> receiver) override;
  void BindDeviceAttributes(
      mojo::PendingReceiver<mojom::DeviceAttributes> receiver) override;
  void BindFileManager(
      mojo::PendingReceiver<mojom::FileManager> receiver) override;
  void BindIdleService(
      mojo::PendingReceiver<mojom::IdleService> receiver) override;
  void BindKeystoreService(
      mojo::PendingReceiver<mojom::KeystoreService> receiver) override;
  void BindMessageCenter(
      mojo::PendingReceiver<mojom::MessageCenter> receiver) override;
  void BindMetricsReporting(
      mojo::PendingReceiver<mojom::MetricsReporting> receiver) override;
  void BindPrefs(mojo::PendingReceiver<mojom::Prefs> receiver) override;
  void BindScreenManager(
      mojo::PendingReceiver<mojom::ScreenManager> receiver) override;
  void BindSelectFile(
      mojo::PendingReceiver<mojom::SelectFile> receiver) override;
  void BindSensorHalClient(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote)
      override;
  void BindHidManager(
      mojo::PendingReceiver<device::mojom::HidManager> receiver) override;
  void BindFeedback(mojo::PendingReceiver<mojom::Feedback> receiver) override;
  void OnBrowserStartup(mojom::BrowserInfoPtr browser_info) override;
  void BindMediaSessionController(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void BindMediaSessionAudioFocus(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override;
  void BindMediaSessionAudioFocusDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override;
  void BindTaskManager(
      mojo::PendingReceiver<mojom::TaskManager> receiver) override;
  void BindTestController(
      mojo::PendingReceiver<mojom::TestController> receiver) override;
  void BindUrlHandler(
      mojo::PendingReceiver<mojom::UrlHandler> receiver) override;
  void BindMachineLearningService(
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService> receiver)
      override;
  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<mojom::VideoCaptureDeviceFactory> receiver)
      override;

  BrowserServiceHostAsh* browser_service_host_ash() {
    return browser_service_host_ash_.get();
  }

  AutomationAsh* automation_ash() { return automation_ash_.get(); }

  TaskManagerAsh* task_manager_ash() { return task_manager_ash_.get(); }

 private:
  // Called when a connection is lost.
  void OnDisconnected();

  std::unique_ptr<AutomationAsh> automation_ash_;
  std::unique_ptr<BrowserServiceHostAsh> browser_service_host_ash_;
  std::unique_ptr<CertDatabaseAsh> cert_database_ash_;
  std::unique_ptr<ClipboardAsh> clipboard_ash_;
  std::unique_ptr<DeviceAttributesAsh> device_attributes_ash_;
  std::unique_ptr<FeedbackAsh> feedback_ash_;
  std::unique_ptr<FileManagerAsh> file_manager_ash_;
  std::unique_ptr<IdleServiceAsh> idle_service_ash_;
  std::unique_ptr<KeystoreServiceAsh> keystore_service_ash_;
  std::unique_ptr<MessageCenterAsh> message_center_ash_;
  std::unique_ptr<MetricsReportingAsh> metrics_reporting_ash_;
  std::unique_ptr<PrefsAsh> prefs_ash_;
  std::unique_ptr<ScreenManagerAsh> screen_manager_ash_;
  std::unique_ptr<SelectFileAsh> select_file_ash_;
  std::unique_ptr<TaskManagerAsh> task_manager_ash_;
  std::unique_ptr<TestControllerAsh> test_controller_ash_;
  std::unique_ptr<UrlHandlerAsh> url_handler_ash_;
  std::unique_ptr<VideoCaptureDeviceFactoryAsh>
      video_capture_device_factory_ash_;

  mojo::ReceiverSet<mojom::Crosapi, CrosapiId> receiver_set_;
  std::map<mojo::ReceiverId, base::OnceClosure> disconnect_handler_map_;

  base::WeakPtrFactory<CrosapiAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ASH_H_
