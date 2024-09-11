// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using extensions::Extension;

namespace {

bool AccessingCamera(Profile* profile, const std::string& app_id) {
  std::optional<bool> accessing_camera;
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->AppCapabilityAccessCache().ForOneApp(
      app_id, [&accessing_camera](const apps::CapabilityAccessUpdate& update) {
        accessing_camera = update.Camera();
      });
  return accessing_camera.value_or(false);
}

bool AccessingMicrophone(Profile* profile, const std::string& app_id) {
  std::optional<bool> accessing_microphone;
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->AppCapabilityAccessCache().ForOneApp(
      app_id,
      [&accessing_microphone](const apps::CapabilityAccessUpdate& update) {
        accessing_microphone = update.Microphone();
      });
  return accessing_microphone.value_or(false);
}

// Adds a fake media device with the specified `stream_type` and starts
// capturing. Returns a closure to stop the capturing.
base::OnceClosure StartMediaCapture(content::WebContents* web_contents,
                                    blink::mojom::MediaStreamType stream_type) {
  blink::mojom::StreamDevices fake_devices;
  blink::MediaStreamDevice device(stream_type, "fake_device", "fake_device");

  if (blink::IsAudioInputMediaType(stream_type)) {
    fake_devices.audio_device = device;
  } else {
    fake_devices.video_device = device;
  }

  std::unique_ptr<content::MediaStreamUI> ui =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          ->RegisterMediaStream(web_contents, fake_devices);

  ui->OnStarted(base::RepeatingClosure(),
                content::MediaStreamUI::SourceCallback(),
                /*label=*/std::string(), /*screen_capture_ids=*/{},
                content::MediaStreamUI::StateChangeCallback());

  return base::BindOnce(
      [](std::unique_ptr<content::MediaStreamUI> ui) { ui.reset(); },
      std::move(ui));
}

}  // namespace

class MediaAccessExtensionAppsTest : public extensions::PlatformAppBrowserTest {
 public:
  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void UninstallApp(const std::string& app_id) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->UninstallSilently(app_id, apps::UninstallSource::kAppList);
  }

  GURL GetUrl1() {
    return embedded_test_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetUrl2() {
    return embedded_test_server()->GetURL("app.com", "/google/google.html");
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(MediaAccessExtensionAppsTest,
                       RequestAccessingForChromeInTabs) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));

  content::WebContents* web_content1 = GetWebContents();
  // Request accessing the camera for |web_content1|.
  base::OnceClosure video_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  AddBlankTabAndShow(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl2()));
  content::WebContents* web_content2 = GetWebContents();
  // Request accessing the microphone for |web_content2|.
  base::OnceClosure audio_closure1 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);

  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_TRUE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Stop accessing the camera for |web_content1|.
  std::move(video_closure1).Run();
  EXPECT_FALSE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_TRUE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Stop accessing the microphone for |web_content2|.
  std::move(audio_closure1).Run();
  EXPECT_FALSE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));
}

IN_PROC_BROWSER_TEST_F(MediaAccessExtensionAppsTest,
                       RequestAccessingForChromeInNewBrowsers) {
  Browser* browser1 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  ASSERT_TRUE(browser1);
  ASSERT_NE(browser(), browser1);

  AddBlankTabAndShow(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, GetUrl1()));
  content::WebContents* web_content1 =
      browser1->tab_strip_model()->GetActiveWebContents();
  // Request accessing the camera and the microphone for |web_content1|.
  base::OnceClosure video_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  base::OnceClosure audio_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_TRUE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  AddBlankTabAndShow(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser1, GetUrl2()));
  content::WebContents* web_content2 =
      browser1->tab_strip_model()->GetActiveWebContents();
  // Request accessing the camera and the microphone for |web_content2|.
  base::OnceClosure video_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  base::OnceClosure audio_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_TRUE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Close the tab for |web_content2|.
  browser1->tab_strip_model()->CloseSelectedTabs();
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_TRUE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));
  std::move(video_closure2).Run();
  std::move(audio_closure2).Run();

  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_TRUE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Close the tab for |web_content1|.
  browser1->tab_strip_model()->CloseSelectedTabs();
  EXPECT_FALSE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  std::move(video_closure1).Run();
  std::move(audio_closure1).Run();

  EXPECT_FALSE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));
}

IN_PROC_BROWSER_TEST_F(MediaAccessExtensionAppsTest,
                       RequestAccessingForPlatformApp) {
  const Extension* extension =
      LoadAndLaunchPlatformApp("context_menu", "Launched");
  ASSERT_TRUE(extension);

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);

  // Request accessing the camera for |web_contents|.
  base::OnceClosure video_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

  EXPECT_TRUE(AccessingCamera(browser()->profile(), extension->id()));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), extension->id()));

  // Request accessing the microphone for |web_contents|.
  base::OnceClosure audio_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);

  EXPECT_TRUE(AccessingCamera(browser()->profile(), extension->id()));
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), extension->id()));

  // Stop accessing the microphone for |web_contents|.
  std::move(audio_closure).Run();

  EXPECT_TRUE(AccessingCamera(browser()->profile(), extension->id()));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), extension->id()));

  // Stop accessing the camera for |web_contents|.
  std::move(video_closure).Run();

  EXPECT_FALSE(AccessingCamera(browser()->profile(), extension->id()));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), extension->id()));
}

IN_PROC_BROWSER_TEST_F(MediaAccessExtensionAppsTest,
                       RequestAccessingForHostApp) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1"));
  ASSERT_TRUE(extension);

  // Navigate to the app's launch URL.
  auto url = extensions::AppLaunchInfo::GetLaunchWebURL(extension);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_content1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_content1);

  // Request accessing the camera and microphone for |web_contents|.
  base::OnceClosure video_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  base::OnceClosure audio_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);

  EXPECT_TRUE(AccessingCamera(browser()->profile(), extension->id()));
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), extension->id()));

  AddBlankTabAndShow(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));
  content::WebContents* web_content2 = GetWebContents();

  // Request accessing the camera for |web_content2|.
  base::OnceClosure video_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

  EXPECT_TRUE(AccessingCamera(browser()->profile(), extension->id()));
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), extension->id()));
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Uninstall the app.
  std::string app_id = extension->id();
  UninstallApp(app_id);

  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id));
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Stop accessing the camera for |web_content2|.
  std::move(video_closure2).Run();

  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id));
  EXPECT_FALSE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));
}

IN_PROC_BROWSER_TEST_F(MediaAccessExtensionAppsTest,
                       RequestAccessingStreamTypesForChromeInTabs) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));

  content::WebContents* web_contents = GetWebContents();
  // Request DEVICE_VIDEO_CAPTURE accessing the camera for |web_contents|.
  base::OnceClosure video_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Request GUM_DESKTOP_VIDEO_CAPTURE accessing the camera for |web_contents|.
  base::OnceClosure desktop_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Stop GUM_DESKTOP_VIDEO_CAPTURE accessing the camera for |web_contents|.
  std::move(desktop_closure).Run();
  EXPECT_TRUE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));

  // Stop DEVICE_VIDEO_CAPTURE accessing the camera for |web_contents|.
  std::move(video_closure).Run();
  EXPECT_FALSE(
      AccessingCamera(browser()->profile(), app_constants::kChromeAppId));
  EXPECT_FALSE(
      AccessingMicrophone(browser()->profile(), app_constants::kChromeAppId));
}

class MediaAccessWebAppsTest : public web_app::WebAppBrowserTestBase {
 public:
  MediaAccessWebAppsTest() = default;
  ~MediaAccessWebAppsTest() override = default;

  std::string CreateWebApp(const GURL& url) const {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    web_app_info->scope = url;
    return web_app::test::InstallWebApp(browser()->profile(),
                                        std::move(web_app_info));
  }

  void UninstallWebApp(const std::string& app_id) const {
    web_app::WebAppTestUninstallObserver app_listener(browser()->profile());
    app_listener.BeginListening();
    web_app::test::UninstallWebApp(browser()->profile(), app_id);
    app_listener.Wait();
  }

  GURL GetUrl1() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetUrl2() {
    return https_server()->GetURL("app.com", "/google/google.html");
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(MediaAccessWebAppsTest, RequestAccessingCamera) {
  std::string app_id = CreateWebApp(GetUrl1());

  // Launch |app_id| in a new tab.
  web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  web_app::NavigateViaLinkClickToURLAndWait(browser(), GetUrl1());

  // Request accessing the camera for |app_id| in the new tab.
  content::WebContents* web_content1 = GetWebContents();
  base::OnceClosure video_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  // Launch |app_id| in a new window.
  content::WebContents* web_content2 = OpenApplication(app_id);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Request accessing the camera for |app_id| in the new window.
  base::OnceClosure video_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  // Stop accessing the camera for |app_id| in the tab.
  std::move(video_closure1).Run();
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  // Stop accessing the camera for |app_id| in the window.
  std::move(video_closure2).Run();
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));

  web_app::CloseAndWait(app_browser);
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));
  web_app::CloseAndWait(browser());
}

IN_PROC_BROWSER_TEST_F(MediaAccessWebAppsTest, RequestAccessingMicrophone) {
  std::string app_id = CreateWebApp(GetUrl1());

  // Launch |app_id| in a new tab.
  web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  web_app::NavigateViaLinkClickToURLAndWait(browser(), GetUrl1());

  // Request accessing the microphone for |app_id| in the new tab.
  content::WebContents* web_content1 = GetWebContents();
  base::OnceClosure audio_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id));

  // Launch |app_id| in a new window.
  content::WebContents* web_content2 = OpenApplication(app_id);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Request accessing the microphone for |app_id| in the new window.
  base::OnceClosure audio_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id));

  // Close browsers.
  web_app::CloseAndWait(app_browser);
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id));

  // Stop accessing the microphone for |app_id| in the tab.
  std::move(audio_closure1).Run();
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id));

  // Stop accessing the microphone for |app_id| in the window.
  std::move(audio_closure2).Run();
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id));
}

IN_PROC_BROWSER_TEST_F(MediaAccessWebAppsTest, RemoveApp) {
  std::string app_id = CreateWebApp(GetUrl1());

  // Launch |app_id| in a new tab.
  web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  web_app::NavigateViaLinkClickToURLAndWait(browser(), GetUrl1());

  // Request accessing the camera and the microphone for |app_id| in the new
  // tab.
  content::WebContents* web_content1 = GetWebContents();
  base::OnceClosure audio_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  base::OnceClosure video_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id));
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  // Launch |app_id| in a new window.
  content::WebContents* web_content2 = OpenApplication(app_id);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Request accessing the camera and the microphone for |app_id| in the new
  // window.
  base::OnceClosure audio_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  base::OnceClosure video_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id));
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  UninstallWebApp(app_id);
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id));

  CreateWebApp(GetUrl1());

  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id));
}

IN_PROC_BROWSER_TEST_F(MediaAccessWebAppsTest, TwoApps) {
  std::string app_id1 = CreateWebApp(GetUrl1());

  // Launch |app_id1| in a new tab.
  web_app::LaunchWebAppBrowser(browser()->profile(), app_id1);
  web_app::NavigateViaLinkClickToURLAndWait(browser(), GetUrl1());

  // Request accessing the camera and the microphone for |app_id1| in the new
  // tab.
  content::WebContents* web_content1 = GetWebContents();
  base::OnceClosure audio_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  base::OnceClosure video_closure1 = StartMediaCapture(
      web_content1, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id1));
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id1));

  std::string app_id2 = CreateWebApp(GetUrl2());

  // Launch |app_id2| in a new window.
  content::WebContents* web_content2 = OpenApplication(app_id2);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Request accessing the camera and the microphone for |app_id2| in the new
  // window.
  base::OnceClosure audio_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  base::OnceClosure video_closure2 = StartMediaCapture(
      web_content2, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id2));
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id2));

  UninstallWebApp(app_id1);
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id1));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id1));
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id2));
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id2));
  std::move(audio_closure1).Run();
  std::move(video_closure1).Run();

  // Stop accessing the camera and the microphone for |app_id2|.
  std::move(audio_closure2).Run();
  std::move(video_closure2).Run();
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id2));
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id2));

  // Navigate to Url1, and check |app_id1| is not accessing the camera or the
  // microphone, because it has been removed.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl1()));
  auto* web_content3 = GetWebContents();
  base::OnceClosure audio_closure3 = StartMediaCapture(
      web_content3, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  base::OnceClosure video_closure3 = StartMediaCapture(
      web_content3, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id1));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id1));
  std::move(audio_closure3).Run();
  std::move(video_closure3).Run();

  // Navigate to Url2, and check |app_id2| is accessing the camera and the
  // microphone.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl2()));
  auto* web_content4 = GetWebContents();
  base::OnceClosure audio_closure4 = StartMediaCapture(
      web_content4, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  base::OnceClosure video_closure4 = StartMediaCapture(
      web_content4, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingMicrophone(browser()->profile(), app_id2));
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id2));
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id1));
  EXPECT_FALSE(AccessingMicrophone(browser()->profile(), app_id1));
}

IN_PROC_BROWSER_TEST_F(MediaAccessWebAppsTest,
                       RequestAccessingCamera_DesktopCaptureAfter) {
  std::string app_id = CreateWebApp(GetUrl1());

  // Launch |app_id| in a new tab.
  web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  web_app::NavigateViaLinkClickToURLAndWait(browser(), GetUrl1());

  // Request DEVICE_VIDEO_CAPTURE accessing the camera for |app_id| in the new
  // tab.
  content::WebContents* web_contents = GetWebContents();
  base::OnceClosure video_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  // Request GUM_DESKTOP_VIDEO_CAPTURE accessing the camera for |app_id| in the
  // new tab.
  base::OnceClosure desktop_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  // Stop DEVICE_VIDEO_CAPTURE accessing the camera for |app_id| in the tab.
  std::move(video_closure).Run();
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));

  // Stop GUM_DESKTOP_VIDEO_CAPTURE accessing the camera for |app_id| in the
  // tab.
  std::move(desktop_closure).Run();
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));

  web_app::CloseAndWait(browser());
}

IN_PROC_BROWSER_TEST_F(MediaAccessWebAppsTest,
                       RequestAccessingCamera_DesktopCaptureBefore) {
  std::string app_id = CreateWebApp(GetUrl1());

  // Launch |app_id| in a new tab.
  web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  web_app::NavigateViaLinkClickToURLAndWait(browser(), GetUrl1());

  // Request GUM_DESKTOP_VIDEO_CAPTURE accessing the camera for |app_id| in the
  // new tab.
  content::WebContents* web_contents = GetWebContents();
  base::OnceClosure desktop_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));

  // Request DEVICE_VIDEO_CAPTURE accessing the camera for |app_id| in the new
  // tab.
  base::OnceClosure video_closure = StartMediaCapture(
      web_contents, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  // Stop GUM_DESKTOP_VIDEO_CAPTURE accessing the camera for |app_id| in the
  // tab.
  std::move(desktop_closure).Run();
  EXPECT_TRUE(AccessingCamera(browser()->profile(), app_id));

  // Stop DEVICE_VIDEO_CAPTURE accessing the camera for |app_id| in the tab.
  std::move(video_closure).Run();
  EXPECT_FALSE(AccessingCamera(browser()->profile(), app_id));

  web_app::CloseAndWait(browser());
}
