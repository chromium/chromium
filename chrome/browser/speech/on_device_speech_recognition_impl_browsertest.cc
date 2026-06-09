// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognition_impl.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
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
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {
constexpr char kEnglishLanguageCode[] = "en-US";
constexpr char kEnglishAlternateLocaleCode[] = "en-AU";
constexpr char kFrenchLanguageCode[] = "fr-FR";
constexpr char kInvalidLanguageCode[] = "xx-XX";

}  // namespace

namespace speech {

class OnDeviceSpeechRecognitionImplBrowserTest : public InProcessBrowserTest {
 public:
  OnDeviceSpeechRecognitionImplBrowserTest()
      : OnDeviceSpeechRecognitionImplBrowserTest(
            std::vector<base::test::FeatureRef>{media::kOnDeviceWebSpeech}) {}
  OnDeviceSpeechRecognitionImplBrowserTest(
      const OnDeviceSpeechRecognitionImplBrowserTest&) = delete;
  OnDeviceSpeechRecognitionImplBrowserTest& operator=(
      const OnDeviceSpeechRecognitionImplBrowserTest&) = delete;
  ~OnDeviceSpeechRecognitionImplBrowserTest() override = default;

  explicit OnDeviceSpeechRecognitionImplBrowserTest(
      const std::vector<base::test::FeatureRef>& enabled_features) {
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

  // InProcessBrowserTest
  void SetUpOnMainThread() override;

  void OnDeviceWebSpeechAvailableCallback(
      media::mojom::AvailabilityStatus actual_status);
  void OnDeviceWebSpeechAvailableCallbackAndAssertStatus(
      media::mojom::AvailabilityStatus expected_status,
      media::mojom::AvailabilityStatus actual_status);
  void Install();
  void InstallCallback(bool expected_success, bool actual_success);
  void WaitUntilAvailable(const std::string& language);
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

void OnDeviceSpeechRecognitionImplBrowserTest::Install() {
  // Install on-device speech recognition and simulate the installation of the
  // SODA library and language pack.
  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));

  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
}

void OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback(
    bool expected_success,
    bool actual_success) {
  ASSERT_EQ(expected_success, actual_success);
}

void OnDeviceSpeechRecognitionImplBrowserTest::WaitUntilAvailable(
    const std::string& language) {
  ASSERT_TRUE(base::test::RunUntil([&]() {
    on_device_speech_recognition()->Available(
        {language}, media::mojom::SpeechRecognitionQuality::kCommand,
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

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest, Available) {
  on_device_speech_recognition()->Available(
      {kInvalidLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       BypassStoragePartitionGuestView) {
  // Create a custom guest site instance, which uses a non-default storage
  // partition.
  scoped_refptr<content::SiteInstance> guest_site_instance =
      content::SiteInstance::CreateForGuest(
          browser()->profile(),
          content::StoragePartitionConfig::Create(
              browser()->profile(), "my_domain", "my_partition", false));

  content::WebContents::CreateParams params(browser()->profile(),
                                            guest_site_instance);
  std::unique_ptr<content::WebContents> guest_contents =
      content::WebContents::Create(params);

  EXPECT_NE(guest_contents->GetPrimaryMainFrame()->GetStoragePartition(),
            browser()->profile()->GetDefaultStoragePartition());

  // Navigate to about:blank directly.
  ASSERT_TRUE(
      content::NavigateToURL(guest_contents.get(), GURL("about:blank")));

  content::RenderFrameHost* main_frame = guest_contents->GetPrimaryMainFrame();
  EXPECT_EQ(GURL("about:blank"), main_frame->GetLastCommittedURL());

  auto* speech_impl =
      OnDeviceSpeechRecognitionImpl::GetOrCreateForCurrentDocument(main_frame);
  ASSERT_TRUE(speech_impl);

  // The vulnerability allows this to be downloadable.
  // A correct implementation would return kUnavailable.
  // We expect it to be kUnavailable to make the test FAIL when the bug is NOT
  // fixed.
  speech_impl->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest, Install) {
  NavigateToUrl("foo.com");

  // Verify that installing an invalid language code returns false.
  on_device_speech_recognition()->Install(
      {kInvalidLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));

  // Verify that on-device speech recognition is downloadable before it is
  // installed.
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
  Install();

  // Verify that on-device speech recognition is available after it is
  // installed.
  WaitUntilAvailable(kEnglishLanguageCode);

  // On-device speech recognition availability is masked by origin, so the
  // previously installed language pack should not be available to a different
  // origin even if it's already installed.
  NavigateToUrl("bar.com");
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  // Verify that on-device speech recognition can be installed on the second
  // origin.
  Install();

  WaitUntilAvailable(kEnglishLanguageCode);

  // Verify that clearing site content settings resets the on-device speech
  // recognition mask for both origins.
  ClearSiteContentSettings();
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       InstallBinaryFailure) {
  NavigateToUrl("foo.com");

  // Verify that a SODA binary installation failure fails the installation
  // callback.
  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));

  speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
      speech::LanguageCode::kNone);
}

// Verify that the `Available()` and `Install()` methods can handle multiple
// languages.
IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       MultipleLanguages) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode, kInvalidLanguageCode},
      media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode, kFrenchLanguageCode},
      media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  WaitUntilAvailable(kEnglishLanguageCode);

  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode, kFrenchLanguageCode},
      media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  on_device_speech_recognition()->Install(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kFrFr);
  WaitUntilAvailable(kFrenchLanguageCode);

  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode, kFrenchLanguageCode},
      media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kAvailable));
}

// Verify that installing different locales of the same language works.
IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       AlternateLocales) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kEnglishAlternateLocaleCode},
      media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  WaitUntilAvailable(kEnglishLanguageCode);

  on_device_speech_recognition()->Available(
      {kEnglishAlternateLocaleCode},
      media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kAvailable));
}

// Verify that passing in empty parameters work as expected.
IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       EmptyParameters) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));

  on_device_speech_recognition()->Install(
      {}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       FileSchemeUrl) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("file:///empty.html")));

  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  WaitUntilAvailable(kEnglishLanguageCode);

  on_device_speech_recognition()->Available(
      {kEnglishAlternateLocaleCode},
      media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kAvailable));
}

class OnDeviceSpeechRecognitionImplGeminiNanoBrowserTest
    : public OnDeviceSpeechRecognitionImplBrowserTest {
 public:
  OnDeviceSpeechRecognitionImplGeminiNanoBrowserTest()
      : OnDeviceSpeechRecognitionImplBrowserTest(
            {media::kOnDeviceWebSpeech, media::kOnDeviceWebSpeechGeminiNano}) {}
};

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplGeminiNanoBrowserTest,
                       AvailableAndInstall) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplGeminiNanoBrowserTest,
                       AvailableAndInstallUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kInvalidLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
  on_device_speech_recognition()->Install(
      {kInvalidLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplGeminiNanoBrowserTest,
                       AvailableUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kInvalidLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplGeminiNanoBrowserTest,
                       InstallUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Install(
      {kInvalidLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));
}

class OnDeviceSpeechRecognitionImplGeminiNanoConversationBrowserTest
    : public OnDeviceSpeechRecognitionImplBrowserTest {
 public:
  OnDeviceSpeechRecognitionImplGeminiNanoConversationBrowserTest()
      : OnDeviceSpeechRecognitionImplBrowserTest(
            {media::kOnDeviceWebSpeech, media::kOnDeviceWebSpeechGeminiNano}) {}
};

IN_PROC_BROWSER_TEST_F(
    OnDeviceSpeechRecognitionImplGeminiNanoConversationBrowserTest,
    AvailableAndInstall) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode},
      media::mojom::SpeechRecognitionQuality::kConversation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode},
      media::mojom::SpeechRecognitionQuality::kConversation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));
}

IN_PROC_BROWSER_TEST_F(
    OnDeviceSpeechRecognitionImplGeminiNanoConversationBrowserTest,
    AvailableAndInstallUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kFrenchLanguageCode},
      media::mojom::SpeechRecognitionQuality::kConversation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
  on_device_speech_recognition()->Install(
      {kFrenchLanguageCode},
      media::mojom::SpeechRecognitionQuality::kConversation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));
}

class OnDeviceSpeechRecognitionImplQualityBrowserTest
    : public OnDeviceSpeechRecognitionImplBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OnDeviceSpeechRecognitionImplQualityBrowserTest()
      : OnDeviceSpeechRecognitionImplBrowserTest(GetFeatures()) {}

  std::vector<base::test::FeatureRef> GetFeatures() {
    return GetParam()
               ? std::vector<
                     base::test::FeatureRef>{media::kOnDeviceWebSpeech,
                                             media::
                                                 kOnDeviceWebSpeechGeminiNano}
               : std::vector<base::test::FeatureRef>{media::kOnDeviceWebSpeech};
  }
};

IN_PROC_BROWSER_TEST_P(OnDeviceSpeechRecognitionImplQualityBrowserTest,
                       AvailableAndInstall) {
  NavigateToUrl("foo.com");
  bool gemini_enabled = GetParam();
  media::mojom::AvailabilityStatus expected_availability =
      gemini_enabled ? media::mojom::AvailabilityStatus::kDownloadable
                     : media::mojom::AvailabilityStatus::kUnavailable;

  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode},
      media::mojom::SpeechRecognitionQuality::kConversation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this), expected_availability));
  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode},
      media::mojom::SpeechRecognitionQuality::kConversation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), gemini_enabled));
}

INSTANTIATE_TEST_SUITE_P(All,
                         OnDeviceSpeechRecognitionImplQualityBrowserTest,
                         ::testing::Bool());

class OnDeviceSpeechRecognitionImplTinyGemmaBrowserTest
    : public OnDeviceSpeechRecognitionImplBrowserTest {
 public:
  OnDeviceSpeechRecognitionImplTinyGemmaBrowserTest()
      : OnDeviceSpeechRecognitionImplBrowserTest(
            {media::kOnDeviceWebSpeech,
             media::kOnDeviceWebSpeechSmallExpertModel}) {}
};

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplTinyGemmaBrowserTest,
                       AvailableAndInstall) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode},
      media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode},
      media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplTinyGemmaBrowserTest,
                       AvailableAndInstallUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
  on_device_speech_recognition()->Install(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplTinyGemmaBrowserTest,
                       AvailableUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplTinyGemmaBrowserTest,
                       InstallUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Install(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));
}

}  // namespace speech
