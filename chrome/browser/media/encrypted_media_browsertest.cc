// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/media/clear_key_cdm_test_helper.h"
#include "chrome/browser/media/media_browsertest.h"
#include "chrome/browser/media/test_license_server.h"
#include "chrome/browser/media/wv_test_license_server_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/cdm/supported_cdm_versions.h"
#include "media/media_buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_WIN)
#include <mfapi.h>

#include "base/win/windows_version.h"
#include "chrome/browser/media/media_foundation_service_monitor.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/win/mf_feature_checks.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
const char kExternalClearKeyInitializeFailKeySystem[] =
    "org.chromium.externalclearkey.initializefail";
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

// Sessions to load.
const char kNoSessionToLoad[] = "";
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
const char kPersistentLicense[] = "PersistentLicense";
const char kUnknownSession[] = "UnknownSession";
#endif

// EME-specific test results and errors.
const char kUnitTestSuccess[] = "UNIT_TEST_SUCCESS";
const char16_t kEmeUnitTestFailure16[] = u"UNIT_TEST_FAILURE";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";
const char16_t kEmeNotSupportedError16[] = u"NOTSUPPORTEDERROR";
const char16_t kEmeGenerateRequestFailed[] = u"EME_GENERATEREQUEST_FAILED";
const char16_t kEmeSessionNotFound16[] = u"EME_SESSION_NOT_FOUND";
const char16_t kEmeLoadFailed[] = u"EME_LOAD_FAILED";
const char kEmeUpdateFailed[] = "EME_UPDATE_FAILED";
const char16_t kEmeUpdateFailed16[] = u"EME_UPDATE_FAILED";
const char16_t kEmeErrorEvent[] = u"EME_ERROR_EVENT";
const char16_t kEmeMessageUnexpectedType[] = u"EME_MESSAGE_UNEXPECTED_TYPE";
const char16_t kEmeRenewalMissingHeader[] = u"EME_RENEWAL_MISSING_HEADER";
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
const char kEmeSessionClosedAndError[] = "EME_SESSION_CLOSED_AND_ERROR";
const char kEmeSessionNotFound[] = "EME_SESSION_NOT_FOUND";
#if BUILDFLAG(IS_CHROMEOS)
const char kEmeUnitTestFailure[] = "UNIT_TEST_FAILURE";
#endif
#endif

const char kDefaultEmePlayer[] = "eme_player.html";
const char kDefaultMseOnlyEmePlayer[] = "mse_different_containers.html";

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_PROPRIETARY_CODECS) && \
    BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
static constexpr wchar_t kDolbyVisionProfile5[] = L"dvhe.05";
static constexpr wchar_t kDolbyVisionProfile8[] = L"dvhe.08";
#endif

// The type of video src used to load media.
enum class SrcType { SRC, MSE };

// Must be in sync with CONFIG_CHANGE_TYPE in eme_player_js/global.js
enum class ConfigChangeType {
  CLEAR_TO_CLEAR = 0,
  CLEAR_TO_ENCRYPTED = 1,
  ENCRYPTED_TO_CLEAR = 2,
  ENCRYPTED_TO_ENCRYPTED = 3,
};

// Whether the video should be not played or played once or twice.
enum class PlayCount { ZERO = 0, ONCE = 1, TWICE = 2 };

using testing::Bool;
using testing::Combine;
using testing::Eq;
using testing::Not;
using testing::Pair;
using testing::UnitTest;
using testing::UnorderedElementsAre;
using testing::Values;
using testing::WithParamInterface;
using ukm::builders::Media_EME_ApiPromiseRejection;
using ukm::builders::Media_EME_CreateMediaKeys;
using ukm::builders::Media_EME_RequestMediaKeySystemAccess;
using ukm::builders::Media_EME_Usage;

// Base class for encrypted media tests.
class EncryptedMediaTestBase : public MediaBrowserTest {
 public:
  // Occasionally these tests are timing out (see crbug.com/1444672).
  // Overriding the setup/run/teardown methods so that the failures will log
  // when these happen to see if we can get a better understanding of what
  // causes the timeout.
  void SetUp() override {
    LOG(INFO) << __func__;
    MediaBrowserTest::SetUp();
  }

  void TearDown() override {
    LOG(INFO) << __func__;
    MediaBrowserTest::TearDown();
  }

  void SetUpOnMainThread() override {
    LOG(INFO) << __func__;
    MediaBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    LOG(INFO) << __func__;
    MediaBrowserTest::TearDownOnMainThread();
  }

  bool RequiresClearKeyCdm(const std::string& key_system) {
    if (key_system == media::kExternalClearKeyKeySystem) {
      return true;
    }
    // Treat `media::kMediaFoundationClearKeyKeySystem` as a separate key system
    // only for Windows
#if BUILDFLAG(IS_WIN)
    if (key_system == media::kMediaFoundationClearKeyKeySystem) {
      return false;
    }
#endif  // BUILDFLAG(IS_WIN)
    std::string prefix = std::string(media::kExternalClearKeyKeySystem) + '.';
    return key_system.substr(0, prefix.size()) == prefix;
  }

#if BUILDFLAG(IS_WIN)
  bool IsMediaFoundationClearKey(const std::string& key_system) {
    return (key_system == media::kMediaFoundationClearKeyKeySystem);
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_WIDEVINE)
  bool IsWidevine(const std::string& key_system) {
    return key_system == kWidevineKeySystem;
  }
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

  void RunEncryptedMediaTestPage(const std::string& html_page,
                                 const std::string& key_system,
                                 const base::StringPairs& query_params,
                                 const std::string& expected_title) {
    base::StringPairs new_query_params = query_params;
    StartLicenseServerIfNeeded(key_system, &new_query_params);
    RunMediaTestPage(html_page, new_query_params, expected_title, true);
  }

  // Tests |html_page| using |media_file| and |key_system|.
  // When |session_to_load| is not empty, the test will try to load
  // |session_to_load| with stored keys, instead of creating a new session
  // and trying to update it with licenses.
  // When |force_invalid_response| is true, the test will provide invalid
  // responses, which should trigger errors.
  // TODO(xhwang): Find an easier way to pass multiple configuration test
  // options.
  void RunEncryptedMediaTest(const std::string& html_page,
                             const std::string& media_file,
                             const std::string& key_system,
                             SrcType src_type,
                             const std::string& session_to_load,
                             bool force_invalid_response,
                             PlayCount play_count,
                             const std::string& expected_title) {
    base::StringPairs query_params;
    query_params.emplace_back("mediaFile", media_file);
    query_params.emplace_back("mediaType",
                              media::GetMimeTypeForFile(media_file));
    query_params.emplace_back("keySystem", key_system);
    if (src_type == SrcType::MSE) {
      query_params.emplace_back("useMSE", "1");
    }
    if (force_invalid_response) {
      query_params.emplace_back("forceInvalidResponse", "1");
    }
    if (!session_to_load.empty()) {
      query_params.emplace_back("sessionToLoad", session_to_load);
    }
    query_params.emplace_back(
        "playCount", base::NumberToString(static_cast<int>(play_count)));
    RunEncryptedMediaTestPage(html_page, key_system, query_params,
                              expected_title);
  }

  void RunSimpleEncryptedMediaTest(const std::string& media_file,
                                   const std::string& key_system,
                                   SrcType src_type,
                                   PlayCount play_count) {
    std::string expected_title = media::kEndedTitle;
    if (!IsPlayBackPossible(key_system)) {
      expected_title = kEmeUpdateFailed;
    }

    RunEncryptedMediaTest(kDefaultEmePlayer, media_file, key_system, src_type,
                          kNoSessionToLoad, false, play_count, expected_title);
    // Check KeyMessage received for all key systems.
    EXPECT_EQ(true, content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        "document.querySelector('video').receivedKeyMessage;"));
  }

  void RunEncryptedMediaMultipleFileTest(const std::string& key_system,
                                         const std::string& video_file,
                                         const std::string& audio_file,
                                         const std::string& expected_title) {
    if (!IsPlayBackPossible(key_system)) {
      LOG(INFO) << "Skipping test - Test requires video playback.";
      return;
    }

    base::StringPairs query_params;
    query_params.emplace_back("keySystem", key_system);
    query_params.emplace_back("runEncrypted", "1");
    if (!video_file.empty()) {
      query_params.emplace_back("videoFile", video_file);
      query_params.emplace_back("videoFormat",
                                media::GetMimeTypeForFile(video_file));
    }
    if (!audio_file.empty()) {
      query_params.emplace_back("audioFile", audio_file);
      query_params.emplace_back("audioFormat",
                                media::GetMimeTypeForFile(audio_file));
    }

    RunEncryptedMediaTestPage(kDefaultMseOnlyEmePlayer, key_system,
                              query_params, expected_title);
  }

  // Starts a license server if available for the |key_system| and adds a
  // 'licenseServerURL' query parameter to |query_params|.
  void StartLicenseServerIfNeeded(const std::string& key_system,
                                  base::StringPairs* query_params) {
    std::unique_ptr<TestLicenseServerConfig> config =
        GetServerConfig(key_system);
    if (!config) {
      return;
    }
    license_server_ = std::make_unique<TestLicenseServer>(std::move(config));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(license_server_->Start());
    }
    query_params->push_back(
        std::make_pair("licenseServerURL", license_server_->GetServerURL()));
  }

  bool IsPlayBackPossible(const std::string& key_system) {
#if BUILDFLAG(ENABLE_WIDEVINE)
    if (IsWidevine(key_system) && !GetServerConfig(key_system)) {
      return false;
    }
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
    return true;
  }

  std::unique_ptr<TestLicenseServerConfig> GetServerConfig(
      const std::string& key_system) {
#if BUILDFLAG(ENABLE_WIDEVINE)
    if (IsWidevine(key_system)) {
      std::unique_ptr<TestLicenseServerConfig> config(
          new WVTestLicenseServerConfig);
      if (config->IsPlatformSupported()) {
        return config;
      }
    }
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
    return nullptr;
  }

 protected:
  std::unique_ptr<TestLicenseServer> license_server_;

  // We want to fail quickly when a test fails because an error is encountered.
  void AddWaitForTitles(content::TitleWatcher* title_watcher) override {
    MediaBrowserTest::AddWaitForTitles(title_watcher);
    title_watcher->AlsoWaitForTitle(kEmeUnitTestFailure16);
    title_watcher->AlsoWaitForTitle(kEmeNotSupportedError16);
    title_watcher->AlsoWaitForTitle(kEmeGenerateRequestFailed);
    title_watcher->AlsoWaitForTitle(kEmeSessionNotFound16);
    title_watcher->AlsoWaitForTitle(kEmeLoadFailed);
    title_watcher->AlsoWaitForTitle(kEmeUpdateFailed16);
    title_watcher->AlsoWaitForTitle(kEmeErrorEvent);
    title_watcher->AlsoWaitForTitle(kEmeMessageUnexpectedType);
    title_watcher->AlsoWaitForTitle(kEmeRenewalMissingHeader);
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaBrowserTest::SetUpCommandLine(command_line);

    // Persistent license is supported on ChromeOS when protected media
    // identifier is allowed which involves a user action. Use this switch to
    // always allow the identifier for testing purpose. Note that the test page
    // is hosted on "127.0.0.1". See net::EmbeddedTestServer for details.
    command_line->AppendSwitchASCII(
        switches::kUnsafelyAllowProtectedMediaIdentifierForDomain, "127.0.0.1");
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->RemoveSwitch(switches::kDisableComponentUpdate);
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  void SetUpCommandLineForKeySystem(
      const std::string& key_system,
      base::CommandLine* command_line,
      const std::vector<base::test::FeatureRefAndParams>&
          enable_additional_features = {}) {
    if (GetServerConfig(key_system)) {
      // Since the web and license servers listen on different ports, we need to
      // disable web-security to send license requests to the license server.
      // TODO(shadi): Add port forwarding to the test web server configuration.
      command_line->AppendSwitch(switches::kDisableWebSecurity);
    }

    // TODO(crbug.com/40787541): WhatsNewUI might be causing timeouts.
    command_line->AppendSwitch(switches::kNoFirstRun);

    std::vector<base::test::FeatureRefAndParams> enabled_features;

    for (auto feature : enable_additional_features) {
      enabled_features.emplace_back(feature);
    }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    if (RequiresClearKeyCdm(key_system)) {
      RegisterClearKeyCdm(command_line);
      enabled_features.push_back({media::kExternalClearKeyForTesting, {}});
    }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN)
    if (IsMediaFoundationClearKey(key_system)) {
      RegisterMediaFoundationClearKeyCdm(enabled_features);
      enabled_features.push_back(
          {media::kHardwareSecureDecryptionExperiment, {}});

      base::FieldTrialParams fallback_params;
      fallback_params["per_site"] = "true";
      enabled_features.emplace_back(media::kHardwareSecureDecryptionFallback,
                                    fallback_params);

      // To enable MediaFoundation playback, tests should run on a hardware GPU
      // other than use a software OpenGL implementation. This can be configured
      // via `switches::kUseGpuInTests` or `--use-gpu-in-tests`.
      if (command_line->HasSwitch(switches::kUseGpuInTests)) {
        // TODO(crbug.com/40896253): Investigate why the video playback doesn't
        // work with `switches::kDisableGpu` and remove this line if possible.
        // For now, `switches::kDisableGpu` should not be set. Otherwise,
        // the video playback will not work with software rendering. Note that
        // this switch is appended to browser_tests.exe by force as a workaround
        // of http://crbug.com/687387.
        command_line->RemoveSwitch(switches::kDisableGpu);
      }
    }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
    enabled_features.push_back({media::kPlatformEncryptedDolbyVision, {}});
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Tests encrypted media playback using ExternalClearKey key system with a
// specific library CDM interface version as test parameter:
// - int: CDM interface version to test
class ECKEncryptedMediaTest : public EncryptedMediaTestBase,
                              public WithParamInterface<int> {
 public:
  int GetCdmInterfaceVersion() { return GetParam(); }

  // We use special |key_system| names to do non-playback related tests,
  // e.g. media::kExternalClearKeyPlatformVerificationTestKeySystem is used to
  // test the platform verification.
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Since we do not test playback, arbitrarily choose a test file and source
    // type.
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm", key_system,
                          SrcType::SRC, kNoSessionToLoad, false,
                          PlayCount::ONCE, expected_title);
  }

  void TestPlaybackCase(const std::string& key_system,
                        const std::string& session_to_load,
                        const std::string& expected_title) {
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-320x240-v_enc-v.webm",
                          key_system, SrcType::MSE, session_to_load, false,
                          PlayCount::ONCE, expected_title);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);

    SetUpCommandLineForKeySystem(media::kExternalClearKeyKeySystem,
                                 command_line);
    // Override enabled CDM interface version for testing.
    command_line->AppendSwitchASCII(
        switches::kOverrideEnabledCdmInterfaceVersion,
        base::NumberToString(GetCdmInterfaceVersion()));
  }
};

// Tests FileIO capabilities in Encrypted Media with the following test
// parameters:
// - int: CDM interface version to test
class ECKEncryptedMediaFileIOTest : public EncryptedMediaTestBase,
                                    public WithParamInterface<int> {
 public:
  int GetCdmInterfaceVersion() { return GetParam(); }

  void TestPlaybackCase(const std::string& key_system,
                        const std::string& session_to_load,
                        const std::string& expected_title) {
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-320x240-v_enc-v.webm",
                          key_system, SrcType::MSE, session_to_load, false,
                          PlayCount::ONCE, expected_title);
  }
  // We use special |key_system| names to do non-playback related tests,
  // e.g. media::kExternalClearKeyFileIOTestKeySystem is used to test file IO.
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Since we do not test playback, arbitrarily choose a test file and source
    // type.
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm", key_system,
                          SrcType::SRC, kNoSessionToLoad, false,
                          PlayCount::ONCE, expected_title);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);

    SetUpCommandLineForKeySystem(media::kExternalClearKeyKeySystem,
                                 command_line);

    // Override enabled CDM interface version for testing.
    command_line->AppendSwitchASCII(
        switches::kOverrideEnabledCdmInterfaceVersion,
        base::NumberToString(GetCdmInterfaceVersion()));
  }
};

// Tests encrypted media playback with output protection using ExternalClearKey
// key system with a specific display surface to be captured specified as the
// test parameter.
class ECKEncryptedMediaOutputProtectionTest
    : public EncryptedMediaTestBase,
      public WithParamInterface<const char*> {
 public:
  void TestOutputProtection(bool create_recorder_before_media_keys) {
#if BUILDFLAG(IS_CHROMEOS)
    // QueryOutputProtectionStatus() is known to fail on Linux Chrome OS builds.
    std::string expected_title = kEmeUnitTestFailure;
#else
    std::string expected_title = kUnitTestSuccess;
#endif

    base::StringPairs query_params;
    if (create_recorder_before_media_keys) {
      query_params.emplace_back("createMediaRecorderBeforeMediaKeys", "1");
    }
    RunMediaTestPage("eme_and_get_display_media.html", query_params,
                     expected_title, /*http=*/true,
                     /*with_transient_activation=*/true);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(media::kExternalClearKeyKeySystem,
                                 command_line);
    // The output protection tests create a MediaRecorder on a MediaStream,
    // so this allows for a fake stream to be created.
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    command_line->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("display-media-type=%s", GetParam()));
  }
};

class ECKIncognitoEncryptedMediaTest : public EncryptedMediaTestBase {
 public:
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Since we do not test playback, arbitrarily choose a test file and source
    // type.
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm", key_system,
                          SrcType::SRC, kNoSessionToLoad, false,
                          PlayCount::ONCE, expected_title);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(media::kExternalClearKeyKeySystem,
                                 command_line);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

class ECKIncognitoEncryptedMediaFileIOTest : public EncryptedMediaTestBase {
 public:
  // We use special |key_system| names to do non-playback related tests,
  // e.g. media::kExternalClearKeyFileIOTestKeySystem is used to test file IO.
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Since we do not test playback, arbitrarily choose a test file and source
    // type.
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm", key_system,
                          SrcType::SRC, kNoSessionToLoad, false,
                          PlayCount::ONCE, expected_title);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(media::kExternalClearKeyKeySystem,
                                 command_line);
    command_line->AppendSwitch(switches::kIncognito);
  }
};
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN)
// Tests encrypted media playback using ClearKey key system while
// the MediaFoundationForClear feature is enabled. This ensures
// proper renderer selection occurs when Media Foundation Renderer
// is set as the default but the playback requires another (e.g.
// default) renderer.
// TODO(crbug.com/40267198): We should create a browser test suite
// intended explicitly for Media Foundation scenarios and move
// the MFClearEncryptedMediaTest tests there.
class MFClearEncryptedMediaTest : public EncryptedMediaTestBase {
 public:
  void TestSimplePlayback(const std::string& encrypted_media) {
    RunSimpleEncryptedMediaTest(encrypted_media, media::kClearKeyKeySystem,
                                SrcType::SRC, PlayCount::ONCE);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    // Add MediaFoundationClearPlayback feature to enablement list.
    std::vector<base::test::FeatureRefAndParams> mf_clear;
    mf_clear.push_back({media::kMediaFoundationClearPlayback, {}});
    SetUpCommandLineForKeySystem(media::kExternalClearKeyKeySystem,
                                 command_line, mf_clear);
  }
};
#endif  // BUILDFLAG(IS_WIN)

// A base class for parameterized encrypted media tests. Subclasses must
// override `CurrentKeySystem()` and `CurrentSourceType()`.
class ParameterizedEncryptedMediaTestBase : public EncryptedMediaTestBase {
 public:
  virtual std::string CurrentKeySystem() = 0;
  virtual SrcType CurrentSourceType() = 0;

  void TestSimplePlayback(const std::string& encrypted_media) {
    RunSimpleEncryptedMediaTest(encrypted_media, CurrentKeySystem(),
                                CurrentSourceType(), PlayCount::ONCE);
  }

  void TestMultiplePlayback(const std::string& encrypted_media) {
    DCHECK(IsPlayBackPossible(CurrentKeySystem()));
    RunEncryptedMediaTest(kDefaultEmePlayer, encrypted_media,
                          CurrentKeySystem(), CurrentSourceType(),
                          kNoSessionToLoad, false, PlayCount::TWICE,
                          media::kEndedTitle);
  }

  void RunInvalidResponseTest() {
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-320x240-av_enc-av.webm",
                          CurrentKeySystem(), CurrentSourceType(),
                          kNoSessionToLoad, true, PlayCount::ONCE,
                          kEmeUpdateFailed);
  }

  void TestFrameSizeChange() {
    RunEncryptedMediaTest("encrypted_frame_size_change.html",
                          "frame_size_change-av_enc-v.webm", CurrentKeySystem(),
                          CurrentSourceType(), kNoSessionToLoad, false,
                          PlayCount::ONCE, media::kEndedTitle);
  }

  void TestConfigChange(ConfigChangeType config_change_type) {
    DCHECK_EQ(CurrentSourceType(), SrcType::MSE)
        << "Config change only happens when using MSE.";
    DCHECK(IsPlayBackPossible(CurrentKeySystem()))
        << "ConfigChange test requires video playback.";

    base::StringPairs query_params;
    query_params.emplace_back("keySystem", CurrentKeySystem());
    query_params.emplace_back(
        "configChangeType",
        base::NumberToString(static_cast<int>(config_change_type)));
    RunEncryptedMediaTestPage("mse_config_change.html", CurrentKeySystem(),
                              query_params, media::kEndedTitle);
  }

  void TestPolicyCheck() {
    base::StringPairs query_params;
    // We do not care about playback so choose an arbitrary media file.
    std::string media_file = "bear-a_enc-a.webm";
    query_params.emplace_back("mediaFile", media_file);
    query_params.emplace_back("mediaType",
                              media::GetMimeTypeForFile(media_file));
    if (CurrentSourceType() == SrcType::MSE) {
      query_params.emplace_back("useMSE", "1");
    }
    query_params.emplace_back("keySystem", CurrentKeySystem());
    query_params.emplace_back("policyCheck", "1");
    RunEncryptedMediaTestPage(kDefaultEmePlayer, CurrentKeySystem(),
                              query_params, kUnitTestSuccess);
  }

  void TestDifferentContainers(const std::string& video_media_file,
                               const std::string& audio_media_file) {
    DCHECK_EQ(CurrentSourceType(), SrcType::MSE);
    RunEncryptedMediaMultipleFileTest(CurrentKeySystem(), video_media_file,
                                      audio_media_file, media::kEndedTitle);
  }

  void DisableEncryptedMedia() {
    PrefService* pref_service = browser()->profile()->GetPrefs();
    pref_service->SetBoolean(prefs::kEnableEncryptedMedia, false);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(CurrentKeySystem(), command_line);
  }
};

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - SrcType: Use MSE or SRC.
//
// Note:
// 1. Only parameterized (*_P) tests can be used. Non-parameterized (*_F)
// tests will crash at GetParam().
// 2. For key systems backed by library CDMs, the latest CDM interface version
// supported by both the CDM and Chromium will be used.
class EncryptedMediaTest
    : public ParameterizedEncryptedMediaTestBase,
      public WithParamInterface<std::tuple<const char*, SrcType>> {
 public:
  std::string CurrentKeySystem() override { return std::get<0>(GetParam()); }
  SrcType CurrentSourceType() override { return std::get<1>(GetParam()); }
};

// Similar to EncryptedMediaTest, but the source type is always MSE. This is
// needed because many tests can only work with MSE (not with SRC), e.g.
// encrypted MP4, see http://crbug.com/170793. Use this class for those tests so
// we don't have to start the test and then skip it.
class MseEncryptedMediaTest : public ParameterizedEncryptedMediaTestBase,
                              public WithParamInterface<const char*> {
 public:
  std::string CurrentKeySystem() override { return GetParam(); }
  SrcType CurrentSourceType() override { return SrcType::MSE; }
};

INSTANTIATE_TEST_SUITE_P(MSE_ClearKey,
                         EncryptedMediaTest,
                         Combine(Values(media::kClearKeyKeySystem),
                                 Values(SrcType::MSE)));

INSTANTIATE_TEST_SUITE_P(MSE_ClearKey,
                         MseEncryptedMediaTest,
                         Values(media::kClearKeyKeySystem));

// External Clear Key is currently only used on platforms that use library CDMs.
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
INSTANTIATE_TEST_SUITE_P(SRC_ExternalClearKey,
                         EncryptedMediaTest,
                         Combine(Values(media::kExternalClearKeyKeySystem),
                                 Values(SrcType::SRC)));

INSTANTIATE_TEST_SUITE_P(MSE_ExternalClearKey,
                         EncryptedMediaTest,
                         Combine(Values(media::kExternalClearKeyKeySystem),
                                 Values(SrcType::MSE)));

INSTANTIATE_TEST_SUITE_P(MSE_ExternalClearKey,
                         MseEncryptedMediaTest,
                         Values(media::kExternalClearKeyKeySystem));
#else   // BUILDFLAG(ENABLE_LIBRARY_CDMS)
// To reduce test time, only run ClearKey SRC tests when we are not running
// ExternalClearKey SRC tests.
INSTANTIATE_TEST_SUITE_P(SRC_ClearKey,
                         EncryptedMediaTest,
                         Combine(Values(media::kClearKeyKeySystem),
                                 Values(SrcType::SRC)));
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM)
INSTANTIATE_TEST_SUITE_P(MSE_Widevine,
                         EncryptedMediaTest,
                         Combine(Values(kWidevineKeySystem),
                                 Values(SrcType::MSE)));

INSTANTIATE_TEST_SUITE_P(MSE_Widevine,
                         MseEncryptedMediaTest,
                         Values(kWidevineKeySystem));
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM)

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-a.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-av.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VP9Video_WebM_Fullsample) {
  TestSimplePlayback("bear-320x240-v-vp9_fullsample_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VP9Video_WebM_Subsample) {
  TestSimplePlayback("bear-320x240-v-vp9_subsample_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM_Opus) {
  TestSimplePlayback("bear-320x240-opus-av_enc-av.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM_Opus) {
  TestSimplePlayback("bear-320x240-opus-av_enc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Multiple_VideoAudio_WebM) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    GTEST_SKIP() << "Playback_Multiple test requires playback.";
  }

  TestMultiplePlayback("bear-320x240-av_enc-av.webm");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, Playback_AudioOnly_MP4_FLAC) {
  TestSimplePlayback("bear-flac-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, Playback_AudioOnly_MP4_OPUS) {
  TestSimplePlayback("bear-opus-cenc.mp4");
}

// TODO(crbug.com/40116010): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       DISABLED_Playback_VideoOnly_MP4_VP9) {
  TestSimplePlayback("bear-320x240-v_frag-vp9-cenc.mp4");
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40197943): Fails on dcheck-enabled builds on 11.0.
#define MAYBE_Playback_VideoOnly_WebM_VP9Profile2 \
  DISABLED_Playback_VideoOnly_WebM_VP9Profile2
#else
#define MAYBE_Playback_VideoOnly_WebM_VP9Profile2 \
  Playback_VideoOnly_WebM_VP9Profile2
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       MAYBE_Playback_VideoOnly_WebM_VP9Profile2) {
  TestSimplePlayback("bear-320x240-v-vp9_profile2_subsample_cenc-v.webm");
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40197943): Fails on dcheck-enabled builds on 11.0.
#define MAYBE_Playback_VideoOnly_MP4_VP9Profile2 \
  DISABLED_Playback_VideoOnly_MP4_VP9Profile2
#else
#define MAYBE_Playback_VideoOnly_MP4_VP9Profile2 \
  Playback_VideoOnly_MP4_VP9Profile2
#endif
IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       MAYBE_Playback_VideoOnly_MP4_VP9Profile2) {
  TestSimplePlayback("bear-320x240-v-vp9_profile2_subsample_cenc-v.mp4");
}

#if BUILDFLAG(ENABLE_AV1_DECODER)

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_AV1) {
  TestSimplePlayback("bear-av1-cenc.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_AV1_10bit) {
  TestSimplePlayback("bear-av1-320x180-10bit-cenc.webm");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, Playback_VideoOnly_MP4_AV1) {
  TestSimplePlayback("bear-av1-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       Playback_VideoOnly_MP4_AV1_10bit) {
  TestSimplePlayback("bear-av1-320x180-10bit-cenc.mp4");
}

#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, InvalidResponseKeyError) {
  RunInvalidResponseTest();
}

// This is not really an "encrypted" media test. Keep it here for completeness.
IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, ConfigChangeVideo_ClearToClear) {
  LOG(INFO) << UnitTest::GetInstance()->current_test_info()->name();
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    GTEST_SKIP() << "ConfigChange test requires video playback.";
  }

  TestConfigChange(ConfigChangeType::CLEAR_TO_CLEAR);
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       ConfigChangeVideo_ClearToEncrypted) {
  LOG(INFO) << UnitTest::GetInstance()->current_test_info()->name();
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    GTEST_SKIP() << "ConfigChange test requires video playback.";
  }

  TestConfigChange(ConfigChangeType::CLEAR_TO_ENCRYPTED);
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       ConfigChangeVideo_EncryptedToClear) {
  LOG(INFO) << UnitTest::GetInstance()->current_test_info()->name();
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    GTEST_SKIP() << "ConfigChange test requires video playback.";
  }

  TestConfigChange(ConfigChangeType::ENCRYPTED_TO_CLEAR);
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       ConfigChangeVideo_EncryptedToEncrypted) {
  LOG(INFO) << UnitTest::GetInstance()->current_test_info()->name();
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    GTEST_SKIP() << "ConfigChange test requires video playback.";
  }

  TestConfigChange(ConfigChangeType::ENCRYPTED_TO_ENCRYPTED);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
  LOG(INFO) << UnitTest::GetInstance()->current_test_info()->name();
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    GTEST_SKIP() << "FrameSizeChange test requires video playback.";
  }

  TestFrameSizeChange();
}

// Only use MSE since this is independent to the demuxer.
IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, PolicyCheck) {
  TestPolicyCheck();
}

// Only use MSE since this is independent to the demuxer.
IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, RemoveTemporarySession) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    GTEST_SKIP() << "RemoveTemporarySession test requires license server.";
  }

  base::StringPairs query_params{{"keySystem", CurrentKeySystem()}};
  RunEncryptedMediaTestPage("eme_remove_session_test.html", CurrentKeySystem(),
                            query_params, media::kEndedTitle);
}

// Only use MSE since this is independent to the demuxer.
IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, EncryptedMediaDisabled) {
  DisableEncryptedMedia();

  // Clear Key key system is always supported.
  std::string expected_title = media::IsClearKey(CurrentKeySystem())
                                   ? media::kEndedTitle
                                   : kEmeNotSupportedError;

  RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm",
                        CurrentKeySystem(), CurrentSourceType(),
                        kNoSessionToLoad, false, PlayCount::ONCE,
                        expected_title);
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, Playback_Check_Ukm) {
  bool is_widevine =
#if BUILDFLAG(ENABLE_WIDEVINE)
      IsWidevine(CurrentKeySystem());
#else
      false;
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TestSimplePlayback("bear-320x240-av_enc-a.webm");

  // Check Media_EME_RequestMediaKeySystemAccess UKM. It is only recorded for
  // Widevine.
  auto request_entries = ukm_recorder.GetEntries(
      Media_EME_RequestMediaKeySystemAccess::kEntryName,
      {Media_EME_RequestMediaKeySystemAccess::kIsAdFrameName,
       Media_EME_RequestMediaKeySystemAccess::kIsCrossOriginName,
       Media_EME_RequestMediaKeySystemAccess::kIsFromMediaCapabilitiesName,
       Media_EME_RequestMediaKeySystemAccess::kIsTopFrameName,
       Media_EME_RequestMediaKeySystemAccess::kKeySystemName,
       Media_EME_RequestMediaKeySystemAccess::kVideoCapabilitiesName,
       Media_EME_RequestMediaKeySystemAccess::
           kVideoCapabilities_HasEmptyRobustnessName,
       Media_EME_RequestMediaKeySystemAccess::
           kVideoCapabilities_HasHwSecureAllRobustnessName});
  if (is_widevine) {
    EXPECT_EQ(request_entries.size(), 1u);
    EXPECT_THAT(
        request_entries[0].metrics,
        UnorderedElementsAre(
            Pair(Media_EME_RequestMediaKeySystemAccess::kIsAdFrameName,
                 /*false=*/0),
            Pair(Media_EME_RequestMediaKeySystemAccess::kIsCrossOriginName,
                 /*false=*/0),
            Pair(Media_EME_RequestMediaKeySystemAccess::
                     kIsFromMediaCapabilitiesName,
                 /*false=*/0),
            Pair(Media_EME_RequestMediaKeySystemAccess::kIsTopFrameName,
                 /*true=*/1),
            Pair(Media_EME_RequestMediaKeySystemAccess::kKeySystemName,
                 /*blink::KeySystemForUkmLegacy::kWidevine=*/1),
            Pair(Media_EME_RequestMediaKeySystemAccess::kVideoCapabilitiesName,
                 /*true=*/1),
            Pair(Media_EME_RequestMediaKeySystemAccess::
                     kVideoCapabilities_HasEmptyRobustnessName,
                 /*true=*/1),
            Pair(Media_EME_RequestMediaKeySystemAccess::
                     kVideoCapabilities_HasHwSecureAllRobustnessName,
                 /*false=*/0)));
  } else {
    // Not Widevine, so nothing should be recorded.
    EXPECT_EQ(request_entries.size(), 0u);
  }

  // Check Media_EME_CreateMediaKeys UKM. It is also only recorded for Widevine.
  auto create_entries =
      ukm_recorder.GetEntries(Media_EME_CreateMediaKeys::kEntryName,
                              {Media_EME_CreateMediaKeys::kIsAdFrameName,
                               Media_EME_CreateMediaKeys::kIsCrossOriginName,
                               Media_EME_CreateMediaKeys::kIsTopFrameName,
                               Media_EME_CreateMediaKeys::kKeySystemName});
  if (is_widevine) {
    EXPECT_EQ(create_entries.size(), 1u);
    EXPECT_THAT(create_entries[0].metrics,
                UnorderedElementsAre(
                    Pair(Media_EME_CreateMediaKeys::kIsAdFrameName,
                         /*false=*/0),
                    Pair(Media_EME_CreateMediaKeys::kIsCrossOriginName,
                         /*false=*/0),
                    Pair(Media_EME_CreateMediaKeys::kIsTopFrameName,
                         /*true=*/1),
                    Pair(Media_EME_CreateMediaKeys::kKeySystemName,
                         /*blink::KeySystemForUkmLegacy::kWidevine=*/1)));
  } else {
    // Not Widevine, so nothing should be recorded.
    EXPECT_EQ(create_entries.size(), 0u);
  }

  // Check Media_EME_Usage UKM. It is recorded for all key systems. Currently
  // only update() will be called during playback.
  auto usage_entries = ukm_recorder.GetEntries(
      Media_EME_Usage::kEntryName,
      {Media_EME_Usage::kApiName, Media_EME_Usage::kIsPersistentSessionName,
       Media_EME_Usage::kKeySystemName,
       Media_EME_Usage::kUseHardwareSecureCodecsName});
  EXPECT_EQ(usage_entries.size(), 1u);
  EXPECT_THAT(
      usage_entries[0].metrics,
      UnorderedElementsAre(
          Pair(Media_EME_Usage::kApiName, /*blink::EmeApiType::kUpdate=*/6),
          Pair(Media_EME_Usage::kIsPersistentSessionName, /*false=*/0),
          Pair(Media_EME_Usage::kKeySystemName,
               media::GetKeySystemIntForUKM(CurrentKeySystem())),
          Pair(Media_EME_Usage::kUseHardwareSecureCodecsName, /*false=*/0)));

  // Check Media_EME_ApiPromiseRejection. With Widevine if there is no license
  // server Update() will fail. With ClearKey a valid response is provided, so
  // there is no failure.
  auto promise_entries = ukm_recorder.GetEntries(
      Media_EME_ApiPromiseRejection::kEntryName,
      {Media_EME_ApiPromiseRejection::kApiName,
       Media_EME_ApiPromiseRejection::kKeySystemName,
       Media_EME_ApiPromiseRejection::kSystemCodeName,
       Media_EME_ApiPromiseRejection::kUseHardwareSecureCodecsName});
  if (is_widevine && !license_server_) {
    // Update() should have failed due to invalid license response.
    EXPECT_EQ(promise_entries.size(), 1u);
    // Media_EME_ApiPromiseRejection::kSystemCodeName is key system
    // implementation specific, so exact value may change.
    EXPECT_THAT(
        promise_entries[0].metrics,
        UnorderedElementsAre(
            Pair(Media_EME_ApiPromiseRejection::kApiName,
                 /*blink::EmeApiType::kUpdate=*/6),
            Pair(Media_EME_ApiPromiseRejection::kKeySystemName,
                 media::GetKeySystemIntForUKM(CurrentKeySystem())),
            Pair(Media_EME_ApiPromiseRejection::kSystemCodeName, Not(Eq(0))),
            Pair(Media_EME_ApiPromiseRejection::kUseHardwareSecureCodecsName,
                 /*false=*/0)));
  } else {
    // No error with ClearKey or with Widevine if a license server is available.
    EXPECT_EQ(promise_entries.size(), 0u);
  }
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, Playback_VideoOnly_MP4) {
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, Playback_VideoOnly_MP4_MDAT) {
  TestSimplePlayback("bear-640x360-v_frag-cenc-mdat.mp4");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest, Playback_Encryption_CBCS) {
  TestSimplePlayback("bear-640x360-v_frag-cbcs.mp4");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       Playback_EncryptedVideo_MP4_ClearAudio_WEBM) {
  TestDifferentContainers("bear-640x360-v_frag-cenc.mp4",
                          "bear-320x240-audio-only.webm");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       Playback_ClearVideo_WEBM_EncryptedAudio_MP4) {
  TestDifferentContainers("bear-320x240-video-only.webm",
                          "bear-640x360-a_frag-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4) {
  TestDifferentContainers("bear-320x240-v_enc-v.webm",
                          "bear-640x360-a_frag-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       Playback_EncryptedVideo_CBCS_EncryptedAudio_CENC) {
  TestDifferentContainers("bear-640x360-v_frag-cbcs.mp4",
                          "bear-640x360-a_frag-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(MseEncryptedMediaTest,
                       Playback_EncryptedVideo_CENC_EncryptedAudio_CBCS) {
  TestDifferentContainers("bear-640x360-v_frag-cenc.mp4",
                          "bear-640x360-a_frag-cbcs.mp4");
}

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

// Test CDM_10 through CDM_11.
static_assert(media::CheckSupportedCdmInterfaceVersions(10, 11),
              "Mismatch between implementation and test coverage");
INSTANTIATE_TEST_SUITE_P(CDM_10, ECKEncryptedMediaTest, Values(10));
INSTANTIATE_TEST_SUITE_P(CDM_11, ECKEncryptedMediaTest, Values(11));
INSTANTIATE_TEST_SUITE_P(CDM_10, ECKEncryptedMediaFileIOTest, Values(10));
INSTANTIATE_TEST_SUITE_P(CDM_11, ECKEncryptedMediaFileIOTest, Values(10));

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, InitializeCDMFail) {
  TestNonPlaybackCases(kExternalClearKeyInitializeFailKeySystem,
                       kEmeNotSupportedError);
}

// TODO(crbug.com/40105240): Failing on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CDMCrashDuringDecode DISABLED_CDMCrashDuringDecode
#else
#define MAYBE_CDMCrashDuringDecode CDMCrashDuringDecode
#endif
// When CDM crashes, we should still get a decode error and all sessions should
// be closed.
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, MAYBE_CDMCrashDuringDecode) {
  TestNonPlaybackCases(media::kExternalClearKeyCrashKeySystem,
                       kEmeSessionClosedAndError);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, PlatformVerificationTest) {
  TestNonPlaybackCases(
      media::kExternalClearKeyPlatformVerificationTestKeySystem,
      kUnitTestSuccess);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaFileIOTest, FileIOTest) {
  TestNonPlaybackCases(media::kExternalClearKeyFileIOTestKeySystem,
                       kUnitTestSuccess);
}

// Intermittent leaks on ASan/LSan runs: crbug.com/889923
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
#define MAYBE_MessageTypeTest DISABLED_MessageTypeTest
#else
#define MAYBE_MessageTypeTest MessageTypeTest
#endif
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, MAYBE_MessageTypeTest) {
  TestPlaybackCase(media::kExternalClearKeyMessageTypeTestKeySystem,
                   kNoSessionToLoad, media::kEndedTitle);
  // Expects 3 message types: 'license-request', 'license-renewal' and
  // 'individualization-request'.
  EXPECT_EQ(3,
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(),
                "document.querySelector('video').receivedMessageTypes.size;"));
}

// Exercises Storage Path, so use ECKEncryptedMediaFileIOTest
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaFileIOTest, LoadPersistentLicense) {
  TestPlaybackCase(media::kExternalClearKeyKeySystem, kPersistentLicense,
                   media::kEndedTitle);
}

// Exercises Storage Path, so use ECKEncryptedMediaFileIOTest
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaFileIOTest, LoadUnknownSession) {
  TestPlaybackCase(media::kExternalClearKeyKeySystem, kUnknownSession,
                   kEmeSessionNotFound);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, LoadSessionAfterClose) {
  base::StringPairs query_params{
      {"keySystem", media::kExternalClearKeyKeySystem}};
  RunEncryptedMediaTestPage("eme_load_session_after_close_test.html",
                            media::kExternalClearKeyKeySystem, query_params,
                            media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, DecryptOnly_VideoAudio_WebM) {
  RunSimpleEncryptedMediaTest("bear-320x240-av_enc-av.webm",
                              media::kExternalClearKeyDecryptOnlyKeySystem,
                              SrcType::MSE, PlayCount::ONCE);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, DecryptOnly_VideoOnly_MP4_VP9) {
  RunSimpleEncryptedMediaTest("bear-320x240-v_frag-vp9-cenc.mp4",
                              media::kExternalClearKeyDecryptOnlyKeySystem,
                              SrcType::MSE, PlayCount::ONCE);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, DecryptOnly_VideoOnly_MP4_CBCS) {
  // 'cbcs' decryption is only supported on CDM 10 or later as long as
  // the appropriate buildflag is enabled.
  std::string expected_result =
      GetCdmInterfaceVersion() >= 10 ? media::kEndedTitle : media::kErrorTitle;
  RunEncryptedMediaTest(kDefaultEmePlayer, "bear-640x360-v_frag-cbcs.mp4",
                        media::kExternalClearKeyDecryptOnlyKeySystem,
                        SrcType::MSE, kNoSessionToLoad, false, PlayCount::ONCE,
                        expected_result);
}

// Encryption Scheme tests. ClearKey key system is covered in
// content/browser/media/encrypted_media_browsertest.cc.
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, Playback_Encryption_CENC) {
  RunEncryptedMediaMultipleFileTest(
      media::kExternalClearKeyKeySystem, "bear-640x360-v_frag-cenc.mp4",
      "bear-640x360-a_frag-cenc.mp4", media::kEndedTitle);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, Playback_Encryption_CBC1) {
  RunEncryptedMediaMultipleFileTest(media::kExternalClearKeyKeySystem,
                                    "bear-640x360-v_frag-cbc1.mp4",
                                    std::string(), media::kErrorTitle);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, Playback_Encryption_CENS) {
  RunEncryptedMediaMultipleFileTest(media::kExternalClearKeyKeySystem,
                                    "bear-640x360-v_frag-cens.mp4",
                                    std::string(), media::kErrorTitle);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, Playback_Encryption_CBCS) {
  // 'cbcs' decryption is only supported on CDM 10 or later as long as
  // the appropriate buildflag is enabled.
  std::string expected_result =
      GetCdmInterfaceVersion() >= 10 ? media::kEndedTitle : media::kErrorTitle;
  RunEncryptedMediaMultipleFileTest(
      media::kExternalClearKeyKeySystem, "bear-640x360-v_frag-cbcs.mp4",
      "bear-640x360-a_frag-cbcs.mp4", expected_result);
}

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, VerifyCdmHostTest) {
  TestNonPlaybackCases(media::kExternalClearKeyVerifyCdmHostTestKeySystem,
                       kUnitTestSuccess);
}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

// TODO(b/326134178): Disable this test on branded builder
// ci/linux_lacros_chrome since it fails.
#if BUILDFLAG(IS_CHROMEOS_LACROS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_StorageIdTest DISABLED_StorageIdTest
#else
#define MAYBE_StorageIdTest StorageIdTest
#endif
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, MAYBE_StorageIdTest) {
  TestNonPlaybackCases(media::kExternalClearKeyStorageIdTestKeySystem,
                       kUnitTestSuccess);
}

// TODO(crbug.com/40601162): Times out in debug builds.
// TODO(crbug.com/40916095, crbug.com/348996697): Test flakiness.
#if !defined(NDEBUG) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_MultipleCdmTypes DISABLED_MultipeCdmTypes
#else
#define MAYBE_MultipleCdmTypes MultipeCdmTypes
#endif
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, MAYBE_MultipleCdmTypes) {
  base::StringPairs empty_query_params;
  RunMediaTestPage("multiple_cdm_types.html", empty_query_params,
                   media::kEndedTitle, true);
}

// Output Protection Tests. Run with different capture inputs. "monitor"
// simulates the whole screen being captured. "window" simulates the Chrome
// window being captured. "browser" simulates the current Chrome tab being
// captured.

INSTANTIATE_TEST_SUITE_P(Capture_Monitor,
                         ECKEncryptedMediaOutputProtectionTest,
                         Values("monitor"));
INSTANTIATE_TEST_SUITE_P(Capture_Window,
                         ECKEncryptedMediaOutputProtectionTest,
                         Values("window"));
INSTANTIATE_TEST_SUITE_P(Capture_Browser,
                         ECKEncryptedMediaOutputProtectionTest,
                         Values("browser"));

// TODO(crbug.com/40671674): Failing on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_BeforeMediaKeys DISABLED_BeforeMediaKeys
#else
#define MAYBE_BeforeMediaKeys BeforeMediaKeys
#endif
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaOutputProtectionTest,
                       MAYBE_BeforeMediaKeys) {
  TestOutputProtection(/*create_recorder_before_media_keys=*/true);
}

// TODO(crbug.com/40671674): Failing on Win.
#if BUILDFLAG(IS_WIN)
#define MAYBE_AfterMediaKeys DISABLED_AfterMediaKeys
#else
#define MAYBE_AfterMediaKeys AfterMediaKeys
#endif
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaOutputProtectionTest,
                       MAYBE_AfterMediaKeys) {
  TestOutputProtection(/*create_recorder_before_media_keys=*/false);
}

// Incognito tests. Ideally we would run all above tests in incognito mode to
// ensure that everything works. However, that would add a lot of extra tests
// that aren't really testing anything different, as normal playback does not
// save anything to disk. Instead we are only running the tests that actually
// have the CDM do file access.

IN_PROC_BROWSER_TEST_F(ECKIncognitoEncryptedMediaFileIOTest, FileIO) {
  // Try the FileIO test using the default CDM API while running in incognito.
  TestNonPlaybackCases(media::kExternalClearKeyFileIOTestKeySystem,
                       kUnitTestSuccess);
}

IN_PROC_BROWSER_TEST_F(ECKIncognitoEncryptedMediaTest, LoadSessionAfterClose) {
  // Loading a session should work in incognito mode.
  base::StringPairs query_params{
      {"keySystem", media::kExternalClearKeyKeySystem}};
  RunEncryptedMediaTestPage("eme_load_session_after_close_test.html",
                            media::kExternalClearKeyKeySystem, query_params,
                            media::kEndedTitle);
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(MFClearEncryptedMediaTest, Playback_AudioClearVideo) {
  TestSimplePlayback("bear-320x240-av_enc-a.webm");
}

IN_PROC_BROWSER_TEST_F(MFClearEncryptedMediaTest, Playback_VideoAudio) {
  TestSimplePlayback("bear-320x240-av_enc-av.webm");
}

IN_PROC_BROWSER_TEST_F(MFClearEncryptedMediaTest, Playback_VideoClearAudio) {
  TestSimplePlayback("bear-320x240-av_enc-v.webm");
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_PROPRIETARY_CODECS)
// MediaFoundation Clear Key Key System uses Windows Media Foundation's decoders
// and H264 is always supported.
class MediaFoundationEncryptedMediaTest : public EncryptedMediaTestBase {
 public:
  void TestMediaFoundationPlayback(const std::string& encrypted_media) {
    RunSimpleEncryptedMediaTest(encrypted_media,
                                media::kMediaFoundationClearKeyKeySystem,
                                SrcType::MSE, PlayCount::ONCE);
  }

  void TestMediaFoundationMultipleFilePlayback(const std::string& video_file,
                                               const std::string& audio_file) {
    std::string expected_title = media::kEndedTitle;
    if (!IsPlayBackPossible(media::kMediaFoundationClearKeyKeySystem)) {
      expected_title = kEmeUpdateFailed;
    }

    base::StringPairs query_params;
    const auto video_format = media::GetMimeTypeForFile(video_file);
    const auto audio_format = media::GetMimeTypeForFile(audio_file);
    const auto media_type =
        media::GetMimeTypeForFile(audio_file + ";" + video_file);
    query_params.emplace_back("keySystem",
                              media::kMediaFoundationClearKeyKeySystem);
    query_params.emplace_back("runEncrypted", "1");
    query_params.emplace_back("useMSE", "1");
    query_params.emplace_back("playCount", "1");
    query_params.emplace_back("videoFile", video_file);
    query_params.emplace_back("videoFormat", video_format);
    query_params.emplace_back("audioFile", audio_file);
    query_params.emplace_back("audioFormat", audio_format);
    query_params.emplace_back("mediaType", media_type);
    RunEncryptedMediaTestPage(kDefaultEmePlayer,
                              media::kMediaFoundationClearKeyKeySystem,
                              query_params, media::kEndedTitle);

    // Check KeyMessage received for all key systems.
    EXPECT_EQ(true, content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        "document.querySelector('video').receivedKeyMessage;"));
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(media::kMediaFoundationClearKeyKeySystem,
                                 command_line);
  }

  bool IsMediaFoundationEncryptedPlaybackSupported() {
    bool is_mediafoundation_encrypted_playback_supported =
        media::SupportMediaFoundationEncryptedPlayback();
    bool use_gpu_in_tests = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseGpuInTests);
    bool disable_gpu = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableGpu);

    const auto& gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();
    const auto& active_gpu = gpu_info.active_gpu();
    LOG(INFO) << "active_gpu.vendor_id=" << active_gpu.vendor_id;
    LOG(INFO) << "active_gpu.device_id=" << active_gpu.device_id;
    LOG(INFO) << "active_gpu.driver_version=" << active_gpu.driver_version;
    LOG(INFO) << "gpu_info.gl_vendor=" << gpu_info.gl_vendor;
    LOG(INFO) << "gpu_info.gl_renderer=" << gpu_info.gl_renderer;
    LOG(INFO) << "switches::kDisableGpuDriverBugWorkarounds="
              << base::CommandLine::ForCurrentProcess()->HasSwitch(
                     switches::kDisableGpuDriverBugWorkarounds);

    bool is_playback_supported =
        is_mediafoundation_encrypted_playback_supported && use_gpu_in_tests &&
        !disable_gpu;
    LOG(INFO) << "is_mediafoundation_encrypted_playback_supported="
              << is_mediafoundation_encrypted_playback_supported
              << ", use_gpu_in_tests=" << use_gpu_in_tests
              << ", disable_gpu=" << disable_gpu;

    // Run test only if the test machine supports MediaFoundation playback.
    // Otherwise, NotSupportedError or the failure to create D3D11 device is
    // expected.
    if (!is_playback_supported) {
      LOG(INFO) << "Test method "
                << UnitTest::GetInstance()->current_test_info()->name()
                << " is inconclusive since MediaFoundation playback is not "
                   "supported.";

      if (!is_mediafoundation_encrypted_playback_supported) {
        auto os_version = static_cast<int>(base::win::GetVersion());
        LOG(INFO) << "os_version=" << os_version;
      }

      if (!use_gpu_in_tests) {
        LOG(INFO) << "MediaFoundation playback will not work without a "
                     "hardware GPU. Use `--use-gpu-in-tests` flag.";
      }
    }

    return is_playback_supported;
  }

  bool IsDefaultAudioOutputDeviceAvailable() {
    auto default_audio_output_device_id =
        media::CoreAudioUtil::GetDefaultOutputDeviceID();
    LOG(INFO) << "default_audio_output_device_id="
              << default_audio_output_device_id;

    if (default_audio_output_device_id.empty()) {
      LOG(INFO) << "No default audio output device available!";
      return false;
    }

    return true;
  }

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
  bool IsVideoDecoderSupported(const GUID& video_decoder_guid) {
    MFT_REGISTER_TYPE_INFO inputInfo{MFMediaType_Video, video_decoder_guid};
    IMFActivate** activates;
    unsigned int numActivates = 0;
    auto result = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT,
                            &inputInfo, nullptr, &activates, &numActivates);
    if (result != S_OK || numActivates == 0) {
      LOG(INFO) << "No decoders found!";
      return false;
    }

    return true;
  }

  bool IsVideoRendererEffectSupported(const wchar_t* profile) {
    bool supported = false;
    IMFActivate** activates = nullptr;
    unsigned int numActivates = 0;
    auto result = MFTEnumEx(MFT_CATEGORY_VIDEO_RENDERER_EFFECT,
                            MFT_ENUM_FLAG_SORTANDFILTER, nullptr, nullptr,
                            &activates, &numActivates);
    if (result != S_OK || numActivates == 0) {
      LOG(INFO) << "No video renderer effect found!";
      return false;
    }

    for (unsigned int i = 0; i < numActivates && !supported; i++) {
      PROPVARIANT var;
      PropVariantInit(&var);
      auto hr = activates[i]->GetItem(MFT_ENUM_VIDEO_RENDERER_EXTENSION_PROFILE,
                                      &var);
      if (hr == S_OK && var.vt == VARTYPE(VT_VECTOR | VT_LPWSTR)) {
        for (unsigned long j = 0; j < var.calpwstr.cElems; j++) {
          auto elem = *(var.calpwstr.pElems + j);
          if (_wcsicmp(elem, profile) == 0) {
            supported = true;
            break;
          }
        }
      }

      PropVariantClear(&var);
    }

    if (*activates) {
      (*activates)->Release();
    }

    return supported;
  }

  // |profile| is a Dolby Vision renderer profile string which has always 7
  // characters in MediaFoundation. For HEVC based profiles, it's either
  // "dvhe.xx" or "dvh1.xx" where "xx" is a two-digit profile number. Note the
  // DolbyVision level is ignored. See "Table 1: Dolby Vision bitstream
  // profiles" at
  // `https://professionalsupport.dolby.com/s/article/What-is-Dolby-Vision-Profile`.
  bool IsDolbyVisionEncryptedPlaybackSupported(const wchar_t* profile) {
    // Dolby Vision video playback requires both HEVC and Dolby Vision extension
    // codecs.
    bool is_hevc_decoder_supported =
        IsVideoDecoderSupported(MFVideoFormat_HEVC);
    bool is_dolbyvision_supported = IsVideoRendererEffectSupported(profile);
    LOG(INFO) << "is_hevc_decoder_supported=" << is_hevc_decoder_supported;
    LOG(INFO) << "is_dolbyvision_supported=" << is_dolbyvision_supported;

    if (!is_hevc_decoder_supported || !is_dolbyvision_supported) {
      LOG(INFO)
          << "Test method "
          << UnitTest::GetInstance()->current_test_info()->name()
          << " is inconclusive since DolbyVisionEncrypted playback is not "
             "supported.";
      return false;
    }

    return true;
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
};

IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       Playback_ClearLeadEncryptedCencVideo_Success) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  TestMediaFoundationPlayback("bear-640x360-v_frag-cenc.mp4");  // H.264
}

IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       Playback_ClearLeadEncryptedCbcsVideo_Success) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  TestMediaFoundationPlayback("bear-640x360-v_frag-cbcs.mp4");  // H.264
}

IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       Playback_EncryptedCencVideoAudio_Success) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  TestMediaFoundationMultipleFilePlayback(
      "bear-640x360-v_frag-cenc.mp4",   // H.264
      "bear-640x360-a_frag-cenc.mp4");  // MP4 AAC
}

IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       Playback_EncryptedCencAudio_Success) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  std::string expected_title = media::kEndedTitle;

  // TODO(crbug.com/40270855): "Activate failed to create mediasink
  // (0xC00D36FA)" kPlaybackError is expected when playing encrypted audio only
  // content if no audio device. Remove this temporary fix for test machines
  // once the permenent solution is implemented (i.e., a null sink for no audio
  // device).
  if (!IsDefaultAudioOutputDeviceAvailable()) {
    LOG(INFO)
        << "Test method "
        << UnitTest::GetInstance()->current_test_info()->name()
        << " is expected to receive an error since there is no default audio "
           "output device.";
    expected_title = media::kErrorTitle;
  }

  RunEncryptedMediaTest(
      kDefaultEmePlayer, "bear-640x360-a_frag-cenc.mp4",  // MP4 AAC audio only
      media::kMediaFoundationClearKeyKeySystem, SrcType::MSE, kNoSessionToLoad,
      false, PlayCount::ONCE, expected_title);
}

IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       Playback_EncryptedAv1CencAudio_MediaTypeUnsupported) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  // MediaFoundation Clear Key Key System doesn't support AV1 videos
  // (codecs-"av01.0.04M.08"). See AddMediaFoundationClearKey() in
  // components/cdm/renderer/key_system_support_update.cc
  RunEncryptedMediaTest(
      kDefaultEmePlayer, "bear-av1-cenc.mp4", /*codecs="av01.0.04M.08"*/
      media::kMediaFoundationClearKeyKeySystem, SrcType::MSE, kNoSessionToLoad,
      false, PlayCount::ONCE, kEmeNotSupportedError);
}

IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       FallbackTest_KeySystemNotSupported) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  // MediaFoundationServiceMonitor gets lazily initialized in
  // media_foundation_widevine_cdm_component_installer which is not call by the
  // browser tests. Lazily initialize it here.
  MediaFoundationServiceMonitor::GetInstance();

  const char* fallback_expected_title = media::kEndedTitle;

  RunMediaTestPage("media_foundation_fallback.html",
                   {{"keySystem", media::kMediaFoundationClearKeyKeySystem}},
                   fallback_expected_title, /*http=*/true);
}

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       Playback_DolbyVisionProfile5CencVideo_Success) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  if (!IsDolbyVisionEncryptedPlaybackSupported(kDolbyVisionProfile5)) {
    GTEST_SKIP() << "DolbyVisionEncryptedPlayback not supported on device.";
  }

  // DolbyVision Profile 5
  TestMediaFoundationPlayback(
      "color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag-cenc.mp4");
}

IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       Playback_DolbyVisionProfile81CencVideo_Success) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  if (!IsDolbyVisionEncryptedPlaybackSupported(kDolbyVisionProfile8)) {
    GTEST_SKIP() << "DolbyVisionEncryptedPlayback not supported on device.";
  }

  // DolbyVision Profile 8.1
  TestMediaFoundationPlayback(
      "color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag-"
      "cenc.mp4");
}

IN_PROC_BROWSER_TEST_F(MediaFoundationEncryptedMediaTest,
                       Playback_DolbyVisionProfile5CencClearLeadVideo_Success) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  if (!IsDolbyVisionEncryptedPlaybackSupported(kDolbyVisionProfile5)) {
    GTEST_SKIP() << "DolbyVisionEncryptedPlayback not supported on device.";
  }

  // DolbyVision Profile 5
  TestMediaFoundationPlayback(
      "color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag-cenc-clearlead-"
      "2sec.mp4");
}

IN_PROC_BROWSER_TEST_F(
    MediaFoundationEncryptedMediaTest,
    Playback_DolbyVisionProfile81CencClearLeadVideo_Success) {
  if (!IsMediaFoundationEncryptedPlaybackSupported()) {
    GTEST_SKIP() << "MediaFoundationEncryptedPlayback not supported on device.";
  }

  if (!IsDolbyVisionEncryptedPlaybackSupported(kDolbyVisionProfile8)) {
    GTEST_SKIP() << "DolbyVisionEncryptedPlayback not supported on device.";
  }

  // DolbyVision Profile 8.1
  TestMediaFoundationPlayback(
      "color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag-"
      "cenc-clearlead-2sec.mp4");
}

#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_PROPRIETARY_CODECS)
