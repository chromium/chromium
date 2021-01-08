// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_ASH_CHROME_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_ASH_CHROME_SERVICE_IMPL_H_

#include <memory>

#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

class AccountManagerAsh;
class CertDatabaseAsh;
class ClipboardAsh;
class FeedbackAsh;
class FileManagerAsh;
class KeystoreServiceAsh;
class MessageCenterAsh;
class MetricsReportingAsh;
class PrefsAsh;
class ScreenManagerAsh;
class SelectFileAsh;
class TestControllerAsh;

// Implementation of AshChromeService. It provides a set of APIs that
// lacros-chrome can call into.
class AshChromeServiceImpl : public mojom::AshChromeService {
 public:
  explicit AshChromeServiceImpl(
      mojo::PendingReceiver<mojom::AshChromeService> pending_receiver);
  ~AshChromeServiceImpl() override;

  // crosapi::mojom::AshChromeService:
  void BindAccountManager(
      mojo::PendingReceiver<mojom::AccountManager> receiver) override;
  void BindCertDatabase(
      mojo::PendingReceiver<mojom::CertDatabase> receiver) override;
  void BindClipboard(mojo::PendingReceiver<mojom::Clipboard> receiver) override;
  void BindFileManager(
      mojo::PendingReceiver<mojom::FileManager> receiver) override;
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
  void BindHidManager(
      mojo::PendingReceiver<device::mojom::HidManager> receiver) override;
  void BindFeedback(mojo::PendingReceiver<mojom::Feedback> receiver) override;
  void OnLacrosStartup(mojom::LacrosInfoPtr lacros_info) override;
  void BindMediaSessionController(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) override;
  void BindMediaSessionAudioFocus(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> receiver)
      override;
  void BindMediaSessionAudioFocusDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          receiver) override;
  void BindTestController(
      mojo::PendingReceiver<mojom::TestController> receiver) override;

 private:
  mojo::Receiver<mojom::AshChromeService> receiver_;

  std::unique_ptr<AccountManagerAsh> account_manager_ash_;
  std::unique_ptr<FileManagerAsh> file_manager_ash_;
  std::unique_ptr<KeystoreServiceAsh> keystore_service_ash_;
  std::unique_ptr<MessageCenterAsh> message_center_ash_;
  std::unique_ptr<MetricsReportingAsh> metrics_reporting_ash_;
  std::unique_ptr<PrefsAsh> prefs_ash_;
  std::unique_ptr<ScreenManagerAsh> screen_manager_ash_;
  std::unique_ptr<SelectFileAsh> select_file_ash_;
  std::unique_ptr<FeedbackAsh> feedback_ash_;
  std::unique_ptr<CertDatabaseAsh> cert_database_ash_;
  std::unique_ptr<TestControllerAsh> test_controller_ash_;
  std::unique_ptr<ClipboardAsh> clipboard_ash_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_ASH_CHROME_SERVICE_IMPL_H_
