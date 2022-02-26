// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/ozone/buildflags.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

using content::DesktopMediaID;
using content::WebContentsMediaCaptureId;

class DesktopCaptureApiTest : public ExtensionApiTest {
 public:
  DesktopCaptureApiTest() {
    DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(&picker_factory_);
  }
  ~DesktopCaptureApiTest() override {
    DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(NULL);
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

// The build flag OZONE_PLATFORM_WAYLAND is only available on
// Linux or ChromeOS, so this simplifies the next set of ifdefs.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(OZONE_PLATFORM_WAYLAND)
#define OZONE_PLATFORM_WAYLAND
#endif  // BUILDFLAG(OZONE_PLATFORM_WAYLAND)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(https://crbug.com/1271673): Crashes on Lacros.
// TODO(https://crbug.com/1271680): Fails on the linux-wayland-rel bot.
// TODO(https://crbug.com/1271711): Fails on Mac.
#if BUILDFLAG(IS_MAC) || defined(OZONE_PLATFORM_WAYLAND) || \
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
  // TODO(crbug.com/805145): Test fails; invalid device IDs being generated.
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
  // TODO(crbug.com/805145): Test fails; invalid device IDs being generated.
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

// TODO(https://crbug.com/1271673): Crashes on Lacros.
// TODO(https://crbug.com/1271680): Fails on the linux-wayland-rel bot.
// TODO(https://crbug.com/1271711): Fails on Mac.
#if BUILDFLAG(IS_MAC) || defined(OZONE_PLATFORM_WAYLAND) || \
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

  bool result;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "getStream()", &result));
  EXPECT_TRUE(result);

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "getStreamWithInvalidId()", &result));
  EXPECT_TRUE(result);

  // Verify that the picker is closed once the tab is closed.
  content::WebContentsDestroyedWatcher destroyed_watcher(web_contents);
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "openPickerDialogAndReturn()", &result));
  EXPECT_TRUE(result);
  EXPECT_TRUE(test_flags[2].picker_created);
  EXPECT_FALSE(test_flags[2].picker_deleted);

  web_contents->Close();
  destroyed_watcher.Wait();
  EXPECT_TRUE(test_flags[2].picker_deleted);
}

}  // namespace extensions
