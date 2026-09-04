// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognition_impl.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
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
#include "testing/gmock/include/gmock/gmock.h"
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
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features = {
          media::kPreemptiveSodaDownload}) {
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
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
      browser()->GetProfile()->GetBrowsingDataRemover();
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
          browser()->GetProfile(),
          content::StoragePartitionConfig::Create(
              browser()->GetProfile(), "my_domain", "my_partition", false));

  content::WebContents::CreateParams params(browser()->GetProfile(),
                                            guest_site_instance);
  std::unique_ptr<content::WebContents> guest_contents =
      content::WebContents::Create(params);

  EXPECT_NE(guest_contents->GetPrimaryMainFrame()->GetStoragePartition(),
            browser()->GetProfile()->GetDefaultStoragePartition());

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

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       BypassPermissionsPolicy) {
  NavigateToUrl("foo.com");

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  ASSERT_TRUE(content::ExecJs(
      main_frame,
      "new Promise(resolve => {"
      "  let iframe = document.createElement('iframe');"
      "  iframe.src = '/empty.html';"
      "  iframe.allow = \"on-device-speech-recognition 'none'\";"
      "  iframe.onload = resolve;"
      "  document.body.appendChild(iframe);"
      "});"));

  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);

  auto* speech_impl =
      OnDeviceSpeechRecognitionImpl::GetOrCreateForCurrentDocument(child_frame);
  ASSERT_TRUE(speech_impl);

  base::test::TestFuture<media::mojom::AvailabilityStatus> future;

  // The vulnerability allows this to be downloadable.
  // A correct implementation would return kUnavailable.
  // We expect it to be kUnavailable to make the test FAIL when the bug is NOT
  // fixed.
  speech_impl->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      future.GetCallback());

  EXPECT_EQ(future.Get(), media::mojom::AvailabilityStatus::kUnavailable);
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

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       AvailableWithMicAndAcceptLanguage) {
  NavigateToUrl("foo.com");

  // Install so it's available but would normally be masked.
  Install();

  // Normally masked on a different origin.
  NavigateToUrl("bar.com");
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  // Grant Mic permission
  GURL url = embedded_https_test_server().GetURL("bar.com", "/empty.html");
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(url, url,
                                      ContentSettingsType::MEDIASTREAM_MIC,
                                      CONTENT_SETTING_ALLOW);

  // Set Accept-Language to English.
  browser()->GetProfile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                              "en-US,en");

  // Now it should be available.
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kAvailable));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       AvailableWithMicAndAcceptLanguageNotInstalled) {
  NavigateToUrl("foo.com");

  // Normally masked.
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  // Grant Mic permission
  GURL url = embedded_https_test_server().GetURL("foo.com", "/empty.html");
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(url, url,
                                      ContentSettingsType::MEDIASTREAM_MIC,
                                      CONTENT_SETTING_ALLOW);

  // Set Accept-Language to French.
  browser()->GetProfile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                              "fr-FR,fr");

  // Still masked because Accept-Language doesn't match the requested English.
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));

  // Set Accept-Language to English.
  browser()->GetProfile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                              "en-US,en");

  // Now it should be downloadable without user activation because it's not
  // installed yet.
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::
                         kDownloadableWithoutUserActivation));
}

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       AvailableWithAcceptLanguageButNoMicPermission) {
  NavigateToUrl("foo.com");

  // Set Accept-Language to English.
  browser()->GetProfile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                              "en-US,en");

  // Block Mic permission
  GURL url = embedded_https_test_server().GetURL("foo.com", "/empty.html");
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetContentSettingDefaultScope(url, url,
                                      ContentSettingsType::MEDIASTREAM_MIC,
                                      CONTENT_SETTING_BLOCK);

  // Still masked because Mic permission is denied, even though Accept-Language
  // matches.
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
}

class OnDeviceSpeechRecognitionImplPreemptiveBrowserTest
    : public OnDeviceSpeechRecognitionImplBrowserTest {
 public:
  OnDeviceSpeechRecognitionImplPreemptiveBrowserTest()
      : OnDeviceSpeechRecognitionImplBrowserTest(
            {media::kPreemptiveSodaDownload},
            {}) {}
};

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplPreemptiveBrowserTest,
                       PreemptiveDownloadUnmasksAvailability) {
  NavigateToUrl("foo.com");

  // Set Accept-Language to English to match the default language.
  browser()->GetProfile()->GetPrefs()->SetString(language::prefs::kAcceptLanguages,
                                              "en-US,en");

  // Install so it's available.
  Install();

  // Normally masked on a different origin, but should be unmasked since it's
  // the preemptive download language and the feature is enabled.
  NavigateToUrl("bar.com");
  on_device_speech_recognition()->Available(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kAvailable));
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

IN_PROC_BROWSER_TEST_F(OnDeviceSpeechRecognitionImplBrowserTest,
                       InstallWhenLanguagePackAlreadyInstalledButBinaryIsNot) {
  NavigateToUrl("foo.com");

  // 1. Simulate the language pack being installed already,
  // but the SODA binary is NOT installed yet.
  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::GetLanguageCode(kEnglishLanguageCode));

  EXPECT_TRUE(
      speech::SodaInstaller::GetInstance()->InstalledLanguages().contains(
          speech::GetLanguageCode(kEnglishLanguageCode)));
  EXPECT_FALSE(speech::SodaInstaller::GetInstance()->IsSodaBinaryInstalled());

  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;

  // 2. Call install.
  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);

  on_device_speech_recognition()->Install(
      {kEnglishLanguageCode}, media::mojom::SpeechRecognitionQuality::kCommand,
      mock_callback.Get());

  // Ensure the callback didn't run synchronously.
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // 3. Now install the SODA binary. This should finally trigger the callback.
  EXPECT_CALL(mock_callback, Run(true)).Times(1);

  speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
      speech::LanguageCode::kNone);
}

class OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelBrowserTest
    : public OnDeviceSpeechRecognitionImplBrowserTest {
 public:
  OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelBrowserTest()
      : OnDeviceSpeechRecognitionImplBrowserTest(
            {media::kOnDeviceWebSpeech,
             media::kOnDeviceWebSpeechSmallExpertModel}) {}
};

class
    OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelMultiLanguageBrowserTest
    : public OnDeviceSpeechRecognitionImplBrowserTest {
 public:
  OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelMultiLanguageBrowserTest()
      : OnDeviceSpeechRecognitionImplBrowserTest(
            {media::kOnDeviceWebSpeech,
             media::kOnDeviceWebSpeechSmallExpertModel,
             media::kOnDeviceWebSpeechSmallExpertModelMultiLanguage}) {
    feature_list_.InitAndEnableFeatureWithParameters(
        media::kOnDeviceWebSpeechSmallExpertModelMultiLanguage,
        {{"languages", "en-US,fr-FR"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelMultiLanguageBrowserTest,
    AvailableAndInstallSupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kDownloadable));
  on_device_speech_recognition()->Install(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), true));
}

IN_PROC_BROWSER_TEST_F(
    OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelMultiLanguageBrowserTest,
    AvailableAndInstallUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kInvalidLanguageCode},
      media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
  on_device_speech_recognition()->Install(
      {kInvalidLanguageCode},
      media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));
}

IN_PROC_BROWSER_TEST_F(
    OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelBrowserTest,
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

IN_PROC_BROWSER_TEST_F(
    OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelBrowserTest,
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

IN_PROC_BROWSER_TEST_F(
    OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelBrowserTest,
    AvailableUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Available(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::
                         OnDeviceWebSpeechAvailableCallbackAndAssertStatus,
                     base::Unretained(this),
                     media::mojom::AvailabilityStatus::kUnavailable));
}

IN_PROC_BROWSER_TEST_F(
    OnDeviceSpeechRecognitionImplSpeechRecognitionSmallExpertModelBrowserTest,
    InstallUnsupportedLanguage) {
  NavigateToUrl("foo.com");
  on_device_speech_recognition()->Install(
      {kFrenchLanguageCode}, media::mojom::SpeechRecognitionQuality::kDictation,
      base::BindOnce(&OnDeviceSpeechRecognitionImplBrowserTest::InstallCallback,
                     base::Unretained(this), false));
}

}  // namespace speech
