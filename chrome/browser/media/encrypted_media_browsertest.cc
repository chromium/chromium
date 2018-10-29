// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
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
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/key_system_names.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/cdm/supported_cdm_versions.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "third_party/libaom/av1_buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "chrome/browser/media/library_cdm_test_helper.h"
#include "media/cdm/cdm_paths.h"
#endif

// Available key systems.
const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Variants of External Clear Key key system to test different scenarios.
// To add a new variant, make sure you also update:
// - media/test/data/eme_player_js/globals.js
// - media/test/data/eme_player_js/player_utils.js
// - AddExternalClearKey() in chrome_key_systems.cc
// - CreateCdmInstance() in clear_key_cdm.cc
const char kExternalClearKeyMessageTypeTestKeySystem[] =
    "org.chromium.externalclearkey.messagetypetest";
const char kExternalClearKeyFileIOTestKeySystem[] =
    "org.chromium.externalclearkey.fileiotest";
const char kExternalClearKeyInitializeFailKeySystem[] =
    "org.chromium.externalclearkey.initializefail";
const char kExternalClearKeyOutputProtectionTestKeySystem[] =
    "org.chromium.externalclearkey.outputprotectiontest";
const char kExternalClearKeyPlatformVerificationTestKeySystem[] =
    "org.chromium.externalclearkey.platformverificationtest";
const char kExternalClearKeyCrashKeySystem[] =
    "org.chromium.externalclearkey.crash";
#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
const char kExternalClearKeyVerifyCdmHostTestKeySystem[] =
    "org.chromium.externalclearkey.verifycdmhosttest";
#endif
const char kExternalClearKeyStorageIdTestKeySystem[] =
    "org.chromium.externalclearkey.storageidtest";
const char kExternalClearKeyCdmProxyKeySystem[] =
    "org.chromium.externalclearkey.cdmproxy";
#endif

// Sessions to load.
const char kNoSessionToLoad[] = "";
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
const char kLoadableSession[] = "LoadableSession";
const char kUnknownSession[] = "UnknownSession";
#endif

// EME-specific test results and errors.
const char kUnitTestSuccess[] = "UNIT_TEST_SUCCESS";
const char kEmeUnitTestFailure[] = "UNIT_TEST_FAILURE";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";
const char kEmeGenerateRequestFailed[] = "EME_GENERATEREQUEST_FAILED";
const char kEmeSessionNotFound[] = "EME_SESSION_NOT_FOUND";
const char kEmeLoadFailed[] = "EME_LOAD_FAILED";
const char kEmeUpdateFailed[] = "EME_UPDATE_FAILED";
const char kEmeErrorEvent[] = "EME_ERROR_EVENT";
const char kEmeMessageUnexpectedType[] = "EME_MESSAGE_UNEXPECTED_TYPE";
const char kEmeRenewalMissingHeader[] = "EME_RENEWAL_MISSING_HEADER";
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
const char kEmeSessionClosedAndError[] = "EME_SESSION_CLOSED_AND_ERROR";
#endif

const char kDefaultEmePlayer[] = "eme_player.html";
const char kDefaultMseOnlyEmePlayer[] = "mse_different_containers.html";

// The type of video src used to load media.
enum class SrcType { SRC, MSE };

// Must be in sync with CONFIG_CHANGE_TYPE in eme_player_js/global.js
enum class ConfigChangeType {
  CLEAR_TO_CLEAR = 0,
  CLEAR_TO_ENCRYPTED = 1,
  ENCRYPTED_TO_CLEAR = 2,
  ENCRYPTED_TO_ENCRYPTED = 3,
};

// Whether the video should be played once or twice.
enum class PlayCount { ONCE, TWICE };

// Base class for encrypted media tests.
class EncryptedMediaTestBase : public MediaBrowserTest {
 public:
  bool IsExternalClearKey(const std::string& key_system) {
    if (key_system == kExternalClearKeyKeySystem)
      return true;
    std::string prefix = std::string(kExternalClearKeyKeySystem) + '.';
    return key_system.substr(0, prefix.size()) == prefix;
  }

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
    if (src_type == SrcType::MSE)
      query_params.emplace_back("useMSE", "1");
    if (force_invalid_response)
      query_params.emplace_back("forceInvalidResponse", "1");
    if (!session_to_load.empty())
      query_params.emplace_back("sessionToLoad", session_to_load);
    if (play_count == PlayCount::TWICE)
      query_params.emplace_back("playTwice", "1");
    RunEncryptedMediaTestPage(html_page, key_system, query_params,
                              expected_title);
  }

  void RunSimpleEncryptedMediaTest(const std::string& media_file,
                                   const std::string& key_system,
                                   SrcType src_type) {
    std::string expected_title = media::kEnded;
    if (!IsPlayBackPossible(key_system)) {
      expected_title = kEmeUpdateFailed;
    }

    RunEncryptedMediaTest(kDefaultEmePlayer, media_file, key_system, src_type,
                          kNoSessionToLoad, false, PlayCount::ONCE,
                          expected_title);
    // Check KeyMessage received for all key systems.
    bool receivedKeyMessage = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send("
        "document.querySelector('video').receivedKeyMessage);",
        &receivedKeyMessage));
    EXPECT_TRUE(receivedKeyMessage);
  }

  void RunEncryptedMediaMultipleFileTest(const std::string& key_system,
                                         const std::string& video_file,
                                         const std::string& audio_file,
                                         const std::string& expected_title) {
    if (!IsPlayBackPossible(key_system)) {
      DVLOG(0) << "Skipping test - Test requires video playback.";
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
    if (!config)
      return;
    license_server_.reset(new TestLicenseServer(std::move(config)));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(license_server_->Start());
    }
    query_params->push_back(
        std::make_pair("licenseServerURL", license_server_->GetServerURL()));
  }

  bool IsPlayBackPossible(const std::string& key_system) {
#if BUILDFLAG(ENABLE_WIDEVINE)
    if (IsWidevine(key_system) && !GetServerConfig(key_system))
      return false;
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
    return true;
  }

  std::unique_ptr<TestLicenseServerConfig> GetServerConfig(
      const std::string& key_system) {
#if BUILDFLAG(ENABLE_WIDEVINE)
    if (IsWidevine(key_system)) {
      std::unique_ptr<TestLicenseServerConfig> config(
          new WVTestLicenseServerConfig);
      if (config->IsPlatformSupported())
        return config;
    }
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
    return nullptr;
  }

 protected:
  std::unique_ptr<TestLicenseServer> license_server_;

  // We want to fail quickly when a test fails because an error is encountered.
  void AddWaitForTitles(content::TitleWatcher* title_watcher) override {
    MediaBrowserTest::AddWaitForTitles(title_watcher);
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeUnitTestFailure));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeNotSupportedError));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kEmeGenerateRequestFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeSessionNotFound));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeLoadFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeUpdateFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeErrorEvent));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kEmeMessageUnexpectedType));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kEmeRenewalMissingHeader));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MediaBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "EncryptedMediaHdcpPolicyCheck");
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  void SetUpCommandLineForKeySystem(const std::string& key_system,
                                    base::CommandLine* command_line) {
    if (GetServerConfig(key_system)) {
      // Since the web and license servers listen on different ports, we need to
      // disable web-security to send license requests to the license server.
      // TODO(shadi): Add port forwarding to the test web server configuration.
      command_line->AppendSwitch(switches::kDisableWebSecurity);
    }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    if (IsExternalClearKey(key_system)) {
      RegisterClearKeyCdm(command_line);
      std::vector<base::Feature> enabled_features = {
          media::kExternalClearKeyForTesting};
      scoped_feature_list_.InitWithFeatures(enabled_features, {});
    }
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Tests encrypted media playback using ExternalClearKey key system with a
// specific library CDM interface version as test parameter:
// - int: CDM interface version to test
class ECKEncryptedMediaTest : public EncryptedMediaTestBase,
                              public testing::WithParamInterface<int> {
 public:
  int GetCdmInterfaceVersion() { return GetParam(); }

  // We use special |key_system| names to do non-playback related tests,
  // e.g. kExternalClearKeyFileIOTestKeySystem is used to test file IO.
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Make sure the Clear Key CDM is properly registered in CdmRegistry.
    EXPECT_TRUE(IsLibraryCdmRegistered(media::kClearKeyCdmGuid));

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
    SetUpCommandLineForKeySystem(kExternalClearKeyKeySystem, command_line);
    // Override enabled CDM interface version for testing.
    command_line->AppendSwitchASCII(
        switches::kOverrideEnabledCdmInterfaceVersion,
        base::IntToString(GetCdmInterfaceVersion()));
  }
};

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

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
    : public EncryptedMediaTestBase,
      public testing::WithParamInterface<std::tuple<const char*, SrcType>> {
 public:
  std::string CurrentKeySystem() { return std::get<0>(GetParam()); }

  SrcType CurrentSourceType() { return std::get<1>(GetParam()); }

  void TestSimplePlayback(const std::string& encrypted_media) {
    RunSimpleEncryptedMediaTest(encrypted_media, CurrentKeySystem(),
                                CurrentSourceType());
  }

  void TestMultiplePlayback(const std::string& encrypted_media) {
    DCHECK(IsPlayBackPossible(CurrentKeySystem()));
    RunEncryptedMediaTest(kDefaultEmePlayer, encrypted_media,
                          CurrentKeySystem(), CurrentSourceType(),
                          kNoSessionToLoad, false, PlayCount::TWICE,
                          media::kEnded);
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
                          PlayCount::ONCE, media::kEnded);
  }

  void TestConfigChange(ConfigChangeType config_change_type) {
    // TODO(xhwang): Even when config change or playback is not supported we
    // still start Chrome only to return directly here. We probably should not
    // run these test cases at all. See http://crbug.com/693288
    if (CurrentSourceType() != SrcType::MSE) {
      DVLOG(0) << "Config change only happens when using MSE.";
      return;
    }
    if (!IsPlayBackPossible(CurrentKeySystem())) {
      DVLOG(0) << "Skipping test - ConfigChange test requires video playback.";
      return;
    }

    base::StringPairs query_params;
    query_params.emplace_back("keySystem", CurrentKeySystem());
    query_params.emplace_back(
        "configChangeType",
        base::IntToString(static_cast<int>(config_change_type)));
    RunEncryptedMediaTestPage("mse_config_change.html", CurrentKeySystem(),
                              query_params, media::kEnded);
  }

  void TestPolicyCheck() {
    base::StringPairs query_params;
    // We do not care about playback so choose an arbitrary media file.
    std::string media_file = "bear-a_enc-a.webm";
    query_params.emplace_back("mediaFile", media_file);
    query_params.emplace_back("mediaType",
                              media::GetMimeTypeForFile(media_file));
    if (CurrentSourceType() == SrcType::MSE)
      query_params.emplace_back("useMSE", "1");
    query_params.emplace_back("keySystem", CurrentKeySystem());
    query_params.emplace_back("policyCheck", "1");
    RunEncryptedMediaTestPage(kDefaultEmePlayer, CurrentKeySystem(),
                              query_params, kUnitTestSuccess);
  }

  void TestDifferentContainers(const std::string& video_media_file,
                               const std::string& audio_media_file) {
    // MP4 without MSE is not support yet, http://crbug.com/170793.
    if (CurrentSourceType() != SrcType::MSE) {
      DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
      return;
    }

    RunEncryptedMediaMultipleFileTest(CurrentKeySystem(), video_media_file,
                                      audio_media_file, media::kEnded);
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

using ::testing::Combine;
using ::testing::Values;

INSTANTIATE_TEST_CASE_P(MSE_ClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem),
                                Values(SrcType::MSE)));

// External Clear Key is currently only used on platforms that use library CDMs.
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
INSTANTIATE_TEST_CASE_P(SRC_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(SrcType::SRC)));

INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(SrcType::MSE)));
#else   // BUILDFLAG(ENABLE_LIBRARY_CDMS)
// To reduce test time, only run ClearKey SRC tests when we are not running
// ExternalClearKey SRC tests.
INSTANTIATE_TEST_CASE_P(SRC_ClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem),
                                Values(SrcType::SRC)));
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(BUNDLE_WIDEVINE_CDM)
INSTANTIATE_TEST_CASE_P(MSE_Widevine,
                        EncryptedMediaTest,
                        Combine(Values(kWidevineKeySystem),
                                Values(SrcType::MSE)));
#endif  // #if BUILDFLAG(BUNDLE_WIDEVINE_CDM)

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

// TODO(xhwang): Test is flaky. https://crbug.com/890124.
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       DISABLED_Playback_Multiple_VideoAudio_WebM) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Playback_Multiple test requires playback.";
    return;
  }
  TestMultiplePlayback("bear-320x240-av_enc-av.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4_FLAC) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-flac-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_VP9) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-320x240-v_frag-vp9-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_VideoOnly_WebM_VP9Profile2) {
#if BUILDFLAG(ENABLE_WIDEVINE)
  // TODO(crbug.com/707128): Update Widevine CDM to support VP9 profile 1/2/3.
  if (IsWidevine(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Widevine CDM does not support VP9 profile 2";
    return;
  }
#endif
  TestSimplePlayback("bear-320x240-v-vp9_profile2_subsample_cenc-v.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_VP9Profile2) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
#if BUILDFLAG(ENABLE_WIDEVINE)
  // TODO(crbug.com/707128): Update Widevine CDM to support VP9 profile 1/2/3.
  if (IsWidevine(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Widevine CDM does not support VP9 profile 2";
    return;
  }
#endif
  TestSimplePlayback("bear-320x240-v-vp9_profile2_subsample_cenc-v.mp4");
}

#if BUILDFLAG(ENABLE_AV1_DECODER)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_AV1) {
#if BUILDFLAG(ENABLE_WIDEVINE)
  // TODO(crbug.com/884845): Update Widevine CDM to support AV1.
  if (IsWidevine(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Widevine CDM does not support AV1";
    return;
  }
#endif
  TestSimplePlayback("bear-av1-cenc.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM_AV1_10bit) {
#if BUILDFLAG(ENABLE_WIDEVINE)
  // TODO(crbug.com/884845): Update Widevine CDM to support AV1.
  if (IsWidevine(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Widevine CDM does not support AV1";
    return;
  }
#endif
  TestSimplePlayback("bear-av1-320x180-10bit-cenc.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_AV1) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
#if BUILDFLAG(ENABLE_WIDEVINE)
  // TODO(crbug.com/884845): Update Widevine CDM to support AV1.
  if (IsWidevine(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Widevine CDM does not support AV1";
    return;
  }
#endif
  TestSimplePlayback("bear-av1-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_AV1_10bit) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
#if BUILDFLAG(ENABLE_WIDEVINE)
  // TODO(crbug.com/884845): Update Widevine CDM to support AV1.
  if (IsWidevine(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Widevine CDM does not support AV1";
    return;
  }
#endif
  TestSimplePlayback("bear-av1-320x180-10bit-cenc.mp4");
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, InvalidResponseKeyError) {
  RunInvalidResponseTest();
}

// Strictly speaking this is not an "encrypted" media test. Keep it here for
// completeness.
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo_ClearToClear) {
  TestConfigChange(ConfigChangeType::CLEAR_TO_CLEAR);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo_ClearToEncrypted) {
  TestConfigChange(ConfigChangeType::CLEAR_TO_ENCRYPTED);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo_EncryptedToClear) {
  TestConfigChange(ConfigChangeType::ENCRYPTED_TO_CLEAR);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       ConfigChangeVideo_EncryptedToEncrypted) {
  TestConfigChange(ConfigChangeType::ENCRYPTED_TO_ENCRYPTED);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - FrameSizeChange test requires video playback.";
    return;
  }
  TestFrameSizeChange();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, PolicyCheck) {
  // There is no need to run this test twice for the same key system.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test.";
    return;
  }

  TestPolicyCheck();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, RemoveTemporarySession) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - RemoveTemporarySession test requires license "
                "server.";
    return;
  }

  // Although this test doesn't play anything, there is no need to run it
  // twice for the same key system.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test.";
    return;
  }

  base::StringPairs query_params{{"keySystem", CurrentKeySystem()}};
  RunEncryptedMediaTestPage("eme_remove_session_test.html", CurrentKeySystem(),
                            query_params, media::kEnded);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, EncryptedMediaDisabled) {
  DisableEncryptedMedia();

  // Clear Key key system is always supported.
  std::string expected_title = media::IsClearKey(CurrentKeySystem())
                                   ? media::kEnded
                                   : kEmeNotSupportedError;

  RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm",
                        CurrentKeySystem(), CurrentSourceType(),
                        kNoSessionToLoad, false, PlayCount::ONCE,
                        expected_title);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4");
}

// Flaky: crbug.com/847881
#if defined(MAC_OSX)
#define MAYBE_Playback_VideoOnly_MP4_MDAT DISABLED_Playback_VideoOnly_MP4_MDAT
#else
#define MAYBE_Playback_VideoOnly_MP4_MDAT Playback_VideoOnly_MP4_MDAT
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, MAYBE_Playback_VideoOnly_MP4_MDAT) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-v_frag-cenc-mdat.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Encryption_CBCS) {
  if (CurrentSourceType() != SrcType::MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }

#if BUILDFLAG(ENABLE_CBCS_ENCRYPTION_SCHEME)
  TestSimplePlayback("bear-640x360-v_frag-cbcs.mp4");
#else
  DVLOG(0) << "Skipping test; 'cbcs' decryption not supported.";
#endif
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_EncryptedVideo_MP4_ClearAudio_WEBM) {
  TestDifferentContainers("bear-640x360-v_frag-cenc.mp4",
                          "bear-320x240-audio-only.webm");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_ClearVideo_WEBM_EncryptedAudio_MP4) {
  TestDifferentContainers("bear-320x240-video-only.webm",
                          "bear-640x360-a_frag-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4) {
  TestDifferentContainers("bear-320x240-v_enc-v.webm",
                          "bear-640x360-a_frag-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_EncryptedVideo_CBCS_EncryptedAudio_CENC) {
  TestDifferentContainers("bear-640x360-v_frag-cbcs.mp4",
                          "bear-640x360-a_frag-cenc.mp4");
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_EncryptedVideo_CENC_EncryptedAudio_CBCS) {
  TestDifferentContainers("bear-640x360-v_frag-cenc.mp4",
                          "bear-640x360-a_frag-cbcs.mp4");
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)

// Test CDM_9 through CDM_11.
static_assert(media::CheckSupportedCdmInterfaceVersions(9, 11),
              "Mismatch between implementation and test coverage");
INSTANTIATE_TEST_CASE_P(CDM_9, ECKEncryptedMediaTest, Values(9));
INSTANTIATE_TEST_CASE_P(CDM_10, ECKEncryptedMediaTest, Values(10));
INSTANTIATE_TEST_CASE_P(CDM_11, ECKEncryptedMediaTest, Values(11));

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, InitializeCDMFail) {
  TestNonPlaybackCases(kExternalClearKeyInitializeFailKeySystem,
                       kEmeNotSupportedError);
}

// When CDM crashes, we should still get a decode error and all sessions should
// be closed.
// Flaky: crbug.com/832800
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, DISABLED_CDMCrashDuringDecode) {
  TestNonPlaybackCases(kExternalClearKeyCrashKeySystem,
                       kEmeSessionClosedAndError);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, FileIOTest) {
  TestNonPlaybackCases(kExternalClearKeyFileIOTestKeySystem, kUnitTestSuccess);
}

// TODO(xhwang): Investigate how to fake capturing activities to test the
// network link detection logic in OutputProtectionProxy.
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, OutputProtectionTest) {
  TestNonPlaybackCases(kExternalClearKeyOutputProtectionTestKeySystem,
                       kUnitTestSuccess);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, PlatformVerificationTest) {
  TestNonPlaybackCases(kExternalClearKeyPlatformVerificationTestKeySystem,
                       kUnitTestSuccess);
}

// Intermittent leaks on ASan/LSan runs: crbug.com/889923
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
#define MAYBE_MessageTypeTest DISABLED_MessageTypeTest
#else
#define MAYBE_MessageTypeTest MessageTypeTest
#endif
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, MAYBE_MessageTypeTest) {
  TestPlaybackCase(kExternalClearKeyMessageTypeTestKeySystem, kNoSessionToLoad,
                   media::kEnded);

  int num_received_message_types = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
      "document.querySelector('video').receivedMessageTypes.size);",
      &num_received_message_types));

  // CDM_9: expects 2 message types 'license-request' and 'license-renewal'.
  // CDM_10 and above: one more message type 'individualization-request'.
  EXPECT_EQ(GetCdmInterfaceVersion() == 9 ? 2 : 3, num_received_message_types);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, LoadLoadableSession) {
  TestPlaybackCase(kExternalClearKeyKeySystem, kLoadableSession, media::kEnded);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, LoadUnknownSession) {
  TestPlaybackCase(kExternalClearKeyKeySystem, kUnknownSession,
                   kEmeSessionNotFound);
}

const char kExternalClearKeyDecryptOnlyKeySystem[] =
    "org.chromium.externalclearkey.decryptonly";

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, DecryptOnly_VideoAudio_WebM) {
  RunSimpleEncryptedMediaTest("bear-320x240-av_enc-av.webm",
                              kExternalClearKeyDecryptOnlyKeySystem,
                              SrcType::MSE);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, DecryptOnly_VideoOnly_MP4_VP9) {
  RunSimpleEncryptedMediaTest("bear-320x240-v_frag-vp9-cenc.mp4",
                              kExternalClearKeyDecryptOnlyKeySystem,
                              SrcType::MSE);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, DecryptOnly_VideoOnly_MP4_CBCS) {
  // 'cbcs' decryption is only supported on CDM 10 or later as long as
  // the appropriate buildflag is enabled.
  std::string expected_result =
      GetCdmInterfaceVersion() >= 10 && BUILDFLAG(ENABLE_CBCS_ENCRYPTION_SCHEME)
          ? media::kEnded
          : media::kError;
  RunEncryptedMediaTest(kDefaultEmePlayer, "bear-640x360-v_frag-cbcs.mp4",
                        kExternalClearKeyDecryptOnlyKeySystem, SrcType::MSE,
                        kNoSessionToLoad, false, PlayCount::ONCE,
                        expected_result);
}

// Encryption Scheme tests. ClearKey key system is covered in
// content/browser/media/encrypted_media_browsertest.cc.
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, Playback_Encryption_CENC) {
  RunEncryptedMediaMultipleFileTest(
      kExternalClearKeyKeySystem, "bear-640x360-v_frag-cenc.mp4",
      "bear-640x360-a_frag-cenc.mp4", media::kEnded);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, Playback_Encryption_CBC1) {
  RunEncryptedMediaMultipleFileTest(kExternalClearKeyKeySystem,
                                    "bear-640x360-v_frag-cbc1.mp4",
                                    std::string(), media::kError);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, Playback_Encryption_CENS) {
  RunEncryptedMediaMultipleFileTest(kExternalClearKeyKeySystem,
                                    "bear-640x360-v_frag-cens.mp4",
                                    std::string(), media::kError);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, Playback_Encryption_CBCS) {
  // 'cbcs' decryption is only supported on CDM 10 or later as long as
  // the appropriate buildflag is enabled.
  std::string expected_result =
      GetCdmInterfaceVersion() >= 10 && BUILDFLAG(ENABLE_CBCS_ENCRYPTION_SCHEME)
          ? media::kEnded
          : media::kError;
  RunEncryptedMediaMultipleFileTest(
      kExternalClearKeyKeySystem, "bear-640x360-v_frag-cbcs.mp4",
      "bear-640x360-a_frag-cbcs.mp4", expected_result);
}

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, VerifyCdmHostTest) {
  TestNonPlaybackCases(kExternalClearKeyVerifyCdmHostTestKeySystem,
                       kUnitTestSuccess);
}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, StorageIdTest) {
  TestNonPlaybackCases(kExternalClearKeyStorageIdTestKeySystem,
                       kUnitTestSuccess);
}

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, MultipleCdmTypes) {
  base::StringPairs empty_query_params;
  RunMediaTestPage("multiple_cdm_types.html", empty_query_params, media::kEnded,
                   true);
}

// Tests that only works on newer CDM interfaces.

IN_PROC_BROWSER_TEST_P(ECKEncryptedMediaTest, CdmProxy) {
  if (GetCdmInterfaceVersion() < 11) {
    DVLOG(0) << "Skipping test; CdmProxy only supported on CDM_11 and above.";
    return;
  }

  // ClearKeyCdmProxy only supports decrypt-only.
  RunSimpleEncryptedMediaTest("bear-a_enc-a.webm",
                              kExternalClearKeyCdmProxyKeySystem, SrcType::MSE);
}

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
