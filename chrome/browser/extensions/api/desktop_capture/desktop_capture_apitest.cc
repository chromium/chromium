// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/base/ozone_buildflags.h"

namespace extensions {

namespace {

using content::DesktopMediaID;
using content::WebContentsMediaCaptureId;
using testing::Combine;
using testing::Values;

class DesktopCaptureApiTest : public ExtensionApiTest {
 public:
  DesktopCaptureApiTest() {
    DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(&picker_factory_);
  }
  ~DesktopCaptureApiTest() override {
    DesktopCaptureChooseDesktopMediaFunction::SetPickerFactoryForTests(nullptr);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::NumberToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
  }

  static DesktopMediaID MakeFakeWebContentsMediaId(bool audio_share) {
    DesktopMediaID media_id(DesktopMediaID::TYPE_WEB_CONTENTS,
                            DesktopMediaID::kNullId,
                            WebContentsMediaCaptureId(DesktopMediaID::kFakeId,
                                                      DesktopMediaID::kFakeId));
    media_id.audio_share = audio_share;
    return media_id;
  }

  FakeDesktopMediaPickerFactory picker_factory_;
};

}  // namespace

// TODO(crbug.com/40805699): Crashes on Lacros.
// TODO(crbug.com/40805704): Fails on the linux-wayland-rel bot.
// TODO(crbug.com/40805725): Fails on Mac.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE_WAYLAND) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ChooseDesktopMedia DISABLED_ChooseDesktopMedia
#else
#define MAYBE_ChooseDesktopMedia ChooseDesktopMedia
#endif
IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, MAYBE_ChooseDesktopMedia) {
  // Each element in the following array corresponds to one test in
  // chrome/test/data/extensions/api_test/desktop_capture/test.js .
  FakeDesktopMediaPickerFactory::TestFlags test_flags[] = {
    // pickerUiCanceled()
    {.expect_screens = true, .expect_windows = true},
    // chooseMedia()
    {.expect_screens = true,
     .expect_windows = true,
     .selected_source =
         DesktopMediaID(DesktopMediaID::TYPE_SCREEN, DesktopMediaID::kNullId)},
    // screensOnly()
    {.expect_screens = true},
    // WindowsOnly()
    {.expect_windows = true},
    // tabOnly()
    {.expect_tabs = true},
    // audioShareNoApproval()
    {.expect_screens = true,
     .expect_windows = true,
     .expect_tabs = true,
     .expect_audio = true,
     .selected_source =
         DesktopMediaID(DesktopMediaID::TYPE_WEB_CONTENTS, 123, false)},
    // audioShareApproval()
    {.expect_screens = true,
     .expect_windows = true,
     .expect_tabs = true,
     .expect_audio = true,
     .selected_source =
         DesktopMediaID(DesktopMediaID::TYPE_WEB_CONTENTS, 123, true)},
    // chooseMediaAndGetStream()
    {.expect_screens = true,
     .expect_windows = true,
     .selected_source = DesktopMediaID(DesktopMediaID::TYPE_SCREEN,
                                       webrtc::kFullDesktopScreenId)},
    // chooseMediaAndTryGetStreamWithInvalidId()
    {.expect_screens = true,
     .expect_windows = true,
     .selected_source = DesktopMediaID(DesktopMediaID::TYPE_SCREEN,
                                       webrtc::kFullDesktopScreenId)},
    // cancelDialog()
    {.expect_screens = true, .expect_windows = true, .cancelled = true},
  // TODO(crbug.com/41366624): Test fails; invalid device IDs being generated.
#if 0
      // tabShareWithAudioPermissionGetStream()
      {.expect_tabs = true,
       .expect_audio = true,
       .selected_source = MakeFakeWebContentsMediaId(true)},
#endif
    // windowShareWithAudioGetStream()
    {.expect_windows = true,
     .expect_audio = true,
     .selected_source = DesktopMediaID(DesktopMediaID::TYPE_WINDOW,
                                       DesktopMediaID::kFakeId, true)},
    // screenShareWithAudioGetStream()
    {.expect_screens = true,
     .expect_audio = true,
     .selected_source = DesktopMediaID(DesktopMediaID::TYPE_SCREEN,
                                       webrtc::kFullDesktopScreenId, true)},
  // TODO(crbug.com/41366624): Test fails; invalid device IDs being generated.
#if 0
      // tabShareWithoutAudioPermissionGetStream()
      {.expect_tabs = true,
       .expect_audio = true,
       .selected_source = MakeFakeWebContentsMediaId(false)},
#endif
    // windowShareWithoutAudioGetStream()
    {.expect_windows = true,
     .expect_audio = true,
     .selected_source =
         DesktopMediaID(DesktopMediaID::TYPE_WINDOW, DesktopMediaID::kFakeId)},
    // screenShareWithoutAudioGetStream()
    {.expect_screens = true,
     .expect_audio = true,
     .selected_source = DesktopMediaID(DesktopMediaID::TYPE_SCREEN,
                                       webrtc::kFullDesktopScreenId)},
  };
  picker_factory_.SetTestFlags(test_flags, std::size(test_flags));
  ASSERT_TRUE(RunExtensionTest("desktop_capture")) << message_;
}

// TODO(crbug.com/40805699): Crashes on Lacros.
// TODO(crbug.com/40805704): Fails on the linux-wayland-rel bot.
// TODO(crbug.com/40805725): Fails on Mac.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE_WAYLAND) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_Delegation DISABLED_Delegation
#else
#define MAYBE_Delegation Delegation
#endif
IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, MAYBE_Delegation) {
  // Initialize test server.
  base::FilePath test_data;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
  embedded_test_server()->ServeFilesFromDirectory(test_data.AppendASCII(
      "extensions/api_test/desktop_capture_delegate"));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load extension.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("desktop_capture_delegate");
  const Extension* extension = LoadExtension(extension_path);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForPath("localhost", "/example.com.html")));

  FakeDesktopMediaPickerFactory::TestFlags test_flags[] = {
      {.expect_screens = true,
       .expect_windows = true,
       .selected_source = DesktopMediaID(DesktopMediaID::TYPE_SCREEN,
                                         webrtc::kFullDesktopScreenId)},
      {.expect_screens = true,
       .expect_windows = true,
       .selected_source = DesktopMediaID(DesktopMediaID::TYPE_SCREEN,
                                         DesktopMediaID::kNullId)},
      {.expect_screens = true,
       .expect_windows = true,
       .selected_source =
           DesktopMediaID(DesktopMediaID::TYPE_SCREEN, DesktopMediaID::kNullId),
       .cancelled = true},
  };
  picker_factory_.SetTestFlags(test_flags, std::size(test_flags));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(web_contents, "getStream()"));

  EXPECT_EQ(true, content::EvalJs(web_contents, "getStreamWithInvalidId()"));

  // Verify that the picker is closed once the tab is closed.
  content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
  EXPECT_EQ(true, content::EvalJs(web_contents, "openPickerDialogAndReturn()"));
  EXPECT_TRUE(test_flags[2].picker_created);
  EXPECT_FALSE(test_flags[2].picker_deleted);

  web_contents->Close();
  destroyed_watcher.Wait();
  EXPECT_TRUE(test_flags[2].picker_deleted);
}

// Not specifying a tab defaults to the extension's background page.
// Service worker-based extensions don't have one, so they must specify
// a tab. This is a regression test for crbug.com/1271590.
IN_PROC_BROWSER_TEST_F(DesktopCaptureApiTest, ServiceWorkerMustSpecifyTab) {
  static constexpr char kManifest[] =
      R"({
           "name": "Desktop Capture",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "worker.js" },
           "permissions": ["desktopCapture"]
         })";

  static constexpr char kWorker[] =
      R"(chrome.test.runTests([
           function noTabIdSpecified() {
             chrome.desktopCapture.chooseDesktopMedia(
               ["screen", "window"],
               function(id) {
                 chrome.test.assertLastError(
                     'A target tab is required when called from a service ' +
                     'worker context.');
                 chrome.test.succeed();
             });
        }]))";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

class DesktopCaptureApiMediaPickerOptionsBaseTest
    : public DesktopCaptureApiTest {
 public:
  DesktopCaptureApiMediaPickerOptionsBaseTest() {
    DesktopCaptureChooseDesktopMediaFunction::SetPickerFactoryForTests(
        &picker_factory_);
  }

  void FromServiceWorker(const std::string& options);

  ~DesktopCaptureApiMediaPickerOptionsBaseTest() override = default;
};

void DesktopCaptureApiMediaPickerOptionsBaseTest::FromServiceWorker(
    const std::string& options) {
  static constexpr char kManifest[] =
      R"({
           "name": "Desktop Capture",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "worker.js" },
           "permissions": ["desktopCapture", "tabs"]
         })";

  const std::string worker = base::StringPrintf(
      R"(chrome.test.runTests([
           function tabIdSpecified() {
             chrome.tabs.query({}, function(tabs) {
               chrome.test.assertTrue(tabs.length == 1);
               chrome.desktopCapture.chooseDesktopMedia(
                 ["tab"], tabs[0],
                 %s
                 function(id) {
                   chrome.test.assertEq("string", typeof id);
                   chrome.test.assertTrue(id != "");
                   chrome.test.succeed();
                 });
             });
        }]))",
      options.c_str());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), worker);

  // Open a tab to capture.
  embedded_test_server()->ServeFilesFromDirectory(GetTestResourcesParentDir());
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURLForPath("localhost", "/test_file.html")));

  FakeDesktopMediaPickerFactory::TestFlags test_flags[] = {
      {.expect_tabs = true,
       .selected_source = MakeFakeWebContentsMediaId(true)},
  };
  picker_factory_.SetTestFlags(test_flags, std::size(test_flags));

  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}

class DesktopCaptureApiMediaPickerWithOptionsTest
    : public DesktopCaptureApiMediaPickerOptionsBaseTest,
      public testing::WithParamInterface<
          std::tuple<std::string, std::string, std::string>> {
 public:
  static std::string ParseParams(
      const std::tuple<std::string, std::string, std::string>& params) {
    std::vector<std::string> options;

    if (!std::get<0>(params).empty()) {
      options.push_back("systemAudio: \"" + std::get<0>(params) + "\"");
    }

    if (!std::get<1>(params).empty()) {
      options.push_back("selfBrowserSurface: \"" + std::get<1>(params) + "\"");
    }

    if (!std::get<2>(params).empty()) {
      options.push_back("suppressLocalAudioPlaybackIntended: " +
                        std::get<2>(params));
    }

    return "{" + base::JoinString(options, ", ") + "},";
  }

  ~DesktopCaptureApiMediaPickerWithOptionsTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(_,
                         DesktopCaptureApiMediaPickerWithOptionsTest,
                         Combine(/*systemAudio*/
                                 Values("", "exclude", "include"),
                                 /*selfBrowserSurface*/
                                 Values("", "exclude", "include"),
                                 /*suppressLocalAudioPlaybackIntended*/
                                 Values("", "false", "true")));

IN_PROC_BROWSER_TEST_P(DesktopCaptureApiMediaPickerWithOptionsTest,
                       FromServiceWorker) {
  FromServiceWorker(ParseParams(GetParam()));
}

class DesktopCaptureApiMediaPickerWithoutOptionsTest
    : public DesktopCaptureApiMediaPickerOptionsBaseTest {
 public:
  ~DesktopCaptureApiMediaPickerWithoutOptionsTest() override = default;
};

IN_PROC_BROWSER_TEST_F(DesktopCaptureApiMediaPickerWithoutOptionsTest,
                       FromServiceWorker) {
  FromServiceWorker("");
}

}  // namespace extensions
