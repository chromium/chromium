// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognition_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {
constexpr char kValidLanguageCode[] = "en-US";
constexpr char kInvalidLanguageCode[] = "xx-XX";

}  // namespace

namespace speech {

class OnDeviceSpeechRecognitionImplBrowserTest : public InProcessBrowserTest {
 public:
  OnDeviceSpeechRecognitionImplBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(media::kOnDeviceWebSpeech);
  }
  OnDeviceSpeechRecognitionImplBrowserTest(
      const OnDeviceSpeechRecognitionImplBrowserTest&) = delete;
  OnDeviceSpeechRecognitionImplBrowserTest& operator=(
      const OnDeviceSpeechRecognitionImplBrowserTest&) = delete;
  ~OnDeviceSpeechRecognitionImplBrowserTest() override = default;

  // InProcessBrowserTest
  void SetUpOnMainThread() override;

  void OnDeviceWebSpeechAvailableCallback(
      media::mojom::AvailabilityStatus actual_status);
  void OnDeviceWebSpeechAvailableCallbackAndAssertStatus(
      media::mojom::AvailabilityStatus expected_status,
      media::mojom::AvailabilityStatus actual_status);
  void InstallOnDeviceSpeechRecognition();
  void InstallOnDeviceSpeechRecognitionCallback(bool expected_success,
                                                bool actual_success);
  void WaitUntilAvailable();
  void NavigateToUrl(const std::string& url_string);
  void ClearSiteContentSettings();
  OnDeviceSpeechRecognitionImpl* on_device_speech_recognition();

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  media::mojom::AvailabilityStatus availability_status_;
};

void OnDeviceSpeechRecognitionImplBrowserTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_https_test_server().ServeFilesFromSourceDirectory(
      GetChromeTestDataDir());
  ASSERT_TRUE(embedded_https_test_server().Start());

  speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();
}

void OnDeviceSpeechRecognitionImplBrowserTest::
    OnDeviceWebSpeechAvailableCallback(
        media::mojom::AvailabilityStatus actual_status) {
  OnDeviceWebSpeechAvailableCallbackAndAssertStatus(actual_status,
                                                    actual_status);
}

void OnDeviceSpeechRecognitionImplBrowserTest::
    OnDeviceWebSpeechAvailableCallbackAndAssertStatus(
        media::mojom::AvailabilityStatus expected_status,
        media::mojom::AvailabilityStatus actual_status) {
  ASSERT_EQ(expected_status, actual_status);
  availability_status_ = actual_status;
}

void OnDeviceSpeechRecognitionImplBrowserTest::
    InstallOnDeviceSpeechRecognition() {
  // Install on-device speech recognition and simulate the installation of the
  // SODA library and language pack.
  on_device_speech_recognition()->InstallOnDeviceSpeechRecognition(
      kValidLanguageCode,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         InstallOnDeviceSpeechRecognitionCallback,
                     base::Unretained(this), true));

  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
}

void OnDeviceSpeechRecognitionImplBrowserTest::
    InstallOnDeviceSpeechRecognitionCallback(bool expected_success,
                                             bool actual_success) {
  ASSERT_EQ(expected_success, actual_success);
}

void OnDeviceSpeechRecognitionImplBrowserTest::WaitUntilAvailable() {
  ASSERT_TRUE(base::test::RunUntil([&]() {
    on_device_speech_recognition()->OnDeviceWebSpeechAvailable(
        kValidLanguageCode,
        base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                           OnDeviceWebSpeechAvailableCallback,
                       base::Unretained(this)));
    return availability_status_ == media::mojom::AvailabilityStatus::kAvailable;
  }));
}

void OnDeviceSpeechRecognitionImplBrowserTest::NavigateToUrl(
    const std::string& url_string) {
  const GURL kUrl(
      embedded_https_test_server().GetURL(url_string, "/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
}

void OnDeviceSpeechRecognitionImplBrowserTest::ClearSiteContentSettings() {
  content::BrowsingDataRemover* remover =
      browser()->profile()->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS,
      chrome_browsing_data_remover::ALL_ORIGIN_TYPES, &observer);
  observer.BlockUntilCompletion();
}

OnDeviceSpeechRecognitionImpl*
OnDeviceSpeechRecognitionImplBrowserTest::on_device_speech_recognition() {
  return OnDeviceSpeechRecognitionImpl::GetOrCreateForCurrentDocument(
      chrome_test_utils::GetActiveWebContents(this)->GetPrimaryMainFrame());
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       OnDeviceWebSpeechAvailable) {
  on_device_speech_recognition()->OnDeviceWebSpeechAvailable(
      kInvalidLanguageCode,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
  on_device_speech_recognition()->OnDeviceWebSpeechAvailable(
      kValidLanguageCode,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       InstallOnDeviceSpeechRecognition) {
  NavigateToUrl("foo.com");

  // Verify that on-device speech recognition is downloadable before it is
  // installed.
  on_device_speech_recognition()->OnDeviceWebSpeechAvailable(
      kValidLanguageCode,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
  InstallOnDeviceSpeechRecognition();

  // Verify that on-device speech recognition is available after it is
  // installed.
  WaitUntilAvailable();

  // On-device speech recognition availability is masked by origin, so the
  // previously installed language pack should not be available to a different
  // origin even if it's already installed.
  NavigateToUrl("bar.com");
  on_device_speech_recognition()->OnDeviceWebSpeechAvailable(
      kValidLanguageCode,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  // Verify that on-device speech recognition can be installed on the second
  // origin.
  InstallOnDeviceSpeechRecognition();

  WaitUntilAvailable();

  // Verify that clearing site content settings resets the on-device speech
  // recognition mask for both origins.
  ClearSiteContentSettings();
  on_device_speech_recognition()->OnDeviceWebSpeechAvailable(
      kValidLanguageCode,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->OnDeviceWebSpeechAvailable(
      kValidLanguageCode,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
}

}  // namespace speech
