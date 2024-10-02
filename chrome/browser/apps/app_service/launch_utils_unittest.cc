// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"

#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps {

class LaunchUtilsTest : public testing::Test {
 protected:
  apps::AppLaunchParams CreateLaunchParams(
      apps::LaunchContainer container,
      WindowOpenDisposition disposition,
      bool preferred_container,
      int64_t display_id = display::kInvalidDisplayId,
      apps::LaunchContainer fallback_container =
          apps::LaunchContainer::kLaunchContainerNone) {
    return apps::CreateAppIdLaunchParamsWithEventFlags(
        app_id, apps::GetEventFlags(disposition, preferred_container),
        apps::LaunchSource::kFromChromeInternal, display_id,
        fallback_container);
  }

  std::string app_id = "aaa";
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(LaunchUtilsTest, WindowContainerAndWindowDisposition) {
  auto container = apps::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, TabContainerAndForegoundTabDisposition) {
  auto container = apps::LaunchContainer::kLaunchContainerTab;
  auto disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, TabContainerAndBackgoundTabDisposition) {
  auto container = apps::LaunchContainer::kLaunchContainerTab;
  auto disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, PreferContainerWithTab) {
  auto container = apps::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  auto preferred_container = apps::LaunchContainer::kLaunchContainerWindow;
  auto params =
      CreateLaunchParams(container, disposition, true,
                         display::kInvalidDisplayId, preferred_container);

  EXPECT_EQ(preferred_container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, PreferContainerWithWindow) {
  auto container = apps::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  auto preferred_container = apps::LaunchContainer::kLaunchContainerWindow;
  auto params =
      CreateLaunchParams(container, disposition, true,
                         display::kInvalidDisplayId, preferred_container);

  EXPECT_EQ(preferred_container, params.container);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB, params.disposition);
}

TEST_F(LaunchUtilsTest, UseIntentFullUrlInLaunchParams) {
  auto disposition = WindowOpenDisposition::NEW_WINDOW;

  const GURL url = GURL("https://example.com/?query=1#frag");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, url);

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, apps::GetEventFlags(disposition, true),
      apps::LaunchSource::kFromChromeInternal, display::kInvalidDisplayId,
      apps::LaunchContainer::kLaunchContainerWindow, std::move(intent),
      &profile_);

  EXPECT_EQ(url, params.override_url);
}

TEST_F(LaunchUtilsTest, IntentFilesAreCopiedToLaunchParams) {
  auto disposition = WindowOpenDisposition::NEW_WINDOW;

  std::vector<apps::IntentFilePtr> files;
  std::string file_path = "filesystem:http://foo.com/test/foo.txt";
  auto file = std::make_unique<apps::IntentFile>(GURL(file_path));
  EXPECT_TRUE(file->url.is_valid());
  file->mime_type = "text/plain";
  files.push_back(std::move(file));
  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                               std::move(files));

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, apps::GetEventFlags(disposition, true),
      apps::LaunchSource::kFromChromeInternal, display::kInvalidDisplayId,
      apps::LaunchContainer::kLaunchContainerWindow, std::move(intent),
      &profile_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(params.launch_files.size(), 1U);
  EXPECT_EQ("foo.txt", params.launch_files[0].MaybeAsASCII());
#else
  ASSERT_EQ(params.launch_files.size(), 0U);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_NoAppID) {
  // Validate an empty vector is returned if there is
  // no AppID specified on the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  EXPECT_EQ(launch_files.size(), 0U);
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_NoFiles) {
  // Validate an empty vector is returned if there are
  // no files specified on the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  EXPECT_EQ(launch_files.size(), 0U);
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_SingleFile) {
  // Validate a vector with size 1 is returned, and the
  // contents match the command line parameter.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 1U);
  EXPECT_EQ(launch_files[0], base::FilePath(FILE_PATH_LITERAL("filename")));
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_MultipleFiles) {
  // Validate a vector with size 2 is returned, and the
  // contents match the command line parameter.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("filename");
  command_line.AppendArg("filename2");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 2U);
  EXPECT_EQ(launch_files[0], base::FilePath(FILE_PATH_LITERAL("filename")));
  EXPECT_EQ(launch_files[1], base::FilePath(FILE_PATH_LITERAL("filename2")));
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_FileProtocol) {
  // Validate a vector with size 1 is returned, and the
  // contents match the command line parameter. This uses
  // the file protocol to reference the file.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("file://filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 1U);
  EXPECT_EQ(launch_files[0],
            base::FilePath(FILE_PATH_LITERAL("file://filename")));
}

// Verifies that a non-file protocol is not treated as a filename.
TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_CustomProtocol) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("web+test://filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  EXPECT_EQ(0U, launch_files.size());
}

#if BUILDFLAG(IS_CHROMEOS)
// Verifies that convert params (with no override url, intent, files) to crosapi
// and back works.
TEST_F(LaunchUtilsTest, ConvertToCrosapi) {
  auto container = apps::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  const int64_t kDisplayId = 1;
  auto params = CreateLaunchParams(container, disposition, false, kDisplayId);

  auto crosapi_params = apps::ConvertLaunchParamsToCrosapi(params, &profile_);
  auto converted_params =
      apps::ConvertCrosapiToLaunchParams(crosapi_params, &profile_);
  EXPECT_EQ(params.app_id, converted_params.app_id);
  EXPECT_EQ(params.container, converted_params.container);
  EXPECT_EQ(params.disposition, converted_params.disposition);
  EXPECT_EQ(params.launch_source, converted_params.launch_source);
  EXPECT_EQ(params.display_id, converted_params.display_id);
}

// Verifies that convert params with override url to crosapi and back works.
TEST_F(LaunchUtilsTest, ConvertToCrosapiUrl) {
  auto container = apps::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  const int64_t kDisplayId = 2;
  auto params = CreateLaunchParams(container, disposition, false, kDisplayId);
  params.override_url = GURL("abc.example.com");

  auto crosapi_params = apps::ConvertLaunchParamsToCrosapi(params, &profile_);
  auto converted_params =
      apps::ConvertCrosapiToLaunchParams(crosapi_params, &profile_);
  EXPECT_EQ(params.app_id, converted_params.app_id);
  EXPECT_EQ(params.container, converted_params.container);
  EXPECT_EQ(params.disposition, converted_params.disposition);
  EXPECT_EQ(params.launch_source, converted_params.launch_source);
  EXPECT_EQ(params.override_url, converted_params.override_url);
  EXPECT_EQ(params.display_id, converted_params.display_id);
}

// Verifies that convert params with files to crosapi and back works.
TEST_F(LaunchUtilsTest, ConvertToCrosapiFiles) {
  auto container = apps::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  const int64_t kDisplayId = 3;
  auto params = CreateLaunchParams(container, disposition, false, kDisplayId);
  params.launch_files.emplace_back("root");

  auto crosapi_params = apps::ConvertLaunchParamsToCrosapi(params, &profile_);
  auto converted_params =
      apps::ConvertCrosapiToLaunchParams(crosapi_params, &profile_);
  EXPECT_EQ(params.app_id, converted_params.app_id);
  EXPECT_EQ(params.container, converted_params.container);
  EXPECT_EQ(params.disposition, converted_params.disposition);
  EXPECT_EQ(params.launch_source, converted_params.launch_source);
  EXPECT_EQ(params.display_id, converted_params.display_id);
  EXPECT_EQ(params.launch_files, converted_params.launch_files);
}

// Verifies that convert params with intent to crosapi and back works.
TEST_F(LaunchUtilsTest, ConvertToCrosapiIntent) {
  auto container = apps::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  const int64_t kDisplayId = 4;
  auto params = CreateLaunchParams(container, disposition, false, kDisplayId);
  params.intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                                 GURL("abc.example.com"));

  auto crosapi_params = apps::ConvertLaunchParamsToCrosapi(params, &profile_);
  auto converted_params =
      apps::ConvertCrosapiToLaunchParams(crosapi_params, &profile_);
  EXPECT_EQ(params.app_id, converted_params.app_id);
  EXPECT_EQ(params.container, converted_params.container);
  EXPECT_EQ(params.disposition, converted_params.disposition);
  EXPECT_EQ(params.launch_source, converted_params.launch_source);
  EXPECT_EQ(params.display_id, converted_params.display_id);
  EXPECT_EQ(*params.intent, *converted_params.intent);
}

// Verifies that convert params from crosapi with incomplete params works.
TEST_F(LaunchUtilsTest, FromCrosapiIncomplete) {
  auto params = crosapi::mojom::LaunchParams::New();
  params->app_id = "aaaa";
  params->launch_source = apps::LaunchSource::kFromIntentUrl;

  auto converted_params = apps::ConvertCrosapiToLaunchParams(params, &profile_);

  EXPECT_EQ(params->app_id, converted_params.app_id);
  EXPECT_EQ(apps::LaunchContainer::kLaunchContainerNone,
            converted_params.container);
  EXPECT_EQ(WindowOpenDisposition::UNKNOWN, converted_params.disposition);
  EXPECT_EQ(apps::LaunchSource::kFromIntentUrl, converted_params.launch_source);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(LaunchUtilsTest, FromCrosapiIntent) {
  constexpr char kIntentMimeType[] = "image/*";
  constexpr char kShareText[] = "Message";
  constexpr char kFilePath[] = "/tmp/picture.png";
  constexpr char kFileMimeType[] = "image/png";
  constexpr char kBaseName[] = "picture.png";

  crosapi::mojom::LaunchParamsPtr crosapi_params =
      crosapi::mojom::LaunchParams::New();
  crosapi_params->container =
      crosapi::mojom::LaunchContainer::kLaunchContainerWindow;
  crosapi_params->disposition =
      crosapi::mojom::WindowOpenDisposition::kNewForegroundTab;
  crosapi_params->launch_source = apps::LaunchSource::kFromSharesheet;
  const int64_t kDisplayId = 5;
  crosapi_params->display_id = kDisplayId;
  crosapi_params->intent = crosapi::mojom::Intent::New();
  crosapi_params->intent->action = apps_util::kIntentActionSend;
  crosapi_params->intent->mime_type = kIntentMimeType;
  crosapi_params->intent->share_text = kShareText;
  {
    std::vector<crosapi::mojom::IntentFilePtr> crosapi_files;
    auto crosapi_file = crosapi::mojom::IntentFile::New();
    crosapi_file->file_path = base::FilePath(kFilePath);
    crosapi_file->mime_type = kFileMimeType;
    crosapi_files.push_back(std::move(crosapi_file));
    crosapi_params->intent->files = std::move(crosapi_files);
  }

  auto converted_params =
      apps::ConvertCrosapiToLaunchParams(crosapi_params, &profile_);

  EXPECT_EQ(converted_params.container,
            apps::LaunchContainer::kLaunchContainerWindow);
  EXPECT_EQ(converted_params.disposition,
            WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(converted_params.launch_source,
            apps::LaunchSource::kFromSharesheet);
  EXPECT_EQ(converted_params.display_id, kDisplayId);

  EXPECT_EQ(converted_params.launch_files.size(), 1U);
  EXPECT_EQ(converted_params.launch_files[0], base::FilePath(kFilePath));

  EXPECT_EQ(converted_params.intent->action, apps_util::kIntentActionSend);
  EXPECT_EQ(converted_params.intent->mime_type, kIntentMimeType);
  EXPECT_EQ(converted_params.intent->share_text, kShareText);
  EXPECT_EQ(converted_params.intent->files.size(), 1U);
  EXPECT_EQ(converted_params.intent->files[0]->file_name,
            base::SafeBaseName::Create(kBaseName));
  EXPECT_EQ(converted_params.intent->files[0]->mime_type, kFileMimeType);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Fake AppPublisher for tracking app launches.
class FakePublisher : public AppPublisher {
 public:
  explicit FakePublisher(AppServiceProxy* proxy) : AppPublisher(proxy) {
    RegisterPublisher(AppType::kWeb);
  }

  void PublishAppWithUrlScope(std::string app_id, GURL scope) {
    AppPtr app = std::make_unique<App>(AppType::kWeb, app_id);
    app->readiness = Readiness::kReady;
    app->handles_intents = true;
    app->intent_filters.push_back(apps_util::MakeIntentFilterForUrlScope(
        scope, /*omit_port_for_testing=*/true));

    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    AppPublisher::Publish(std::move(apps), AppType::kWeb,
                          /*should_notify_initialized=*/true);
  }

  const std::string& last_launched_app() { return last_launched_app_; }

  // AppPublisher:
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override {}
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override {}

  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           IntentPtr intent,
                           LaunchSource launch_source,
                           WindowInfoPtr window_info,
                           LaunchCallback callback) override {
    last_launched_app_ = app_id;
  }

 private:
  std::string last_launched_app_;
};

class MockNewWindowDelegate
    : public testing::NiceMock<ash::TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class LaunchUtilsNewWindowTest : public LaunchUtilsTest {
 public:
  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

 private:
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(LaunchUtilsNewWindowTest,
       MaybeLaunchPreferredAppForUrl_LaunchesPreferred) {
  FakePublisher publisher(AppServiceProxyFactory::GetForProfile(&profile_));
  publisher.PublishAppWithUrlScope("abc", GURL("https://www.example.com/"));
  ASSERT_EQ(test::EnableLinkCapturingByUser(&profile_, "abc"), base::ok());

  MaybeLaunchPreferredAppForUrl(&profile_, GURL("https://www.example.com/foo/"),
                                LaunchSource::kFromTest);

  ASSERT_EQ(publisher.last_launched_app(), "abc");
}

TEST_F(LaunchUtilsNewWindowTest,
       MaybeLaunchPreferredAppForUrl_LaunchesBrowserIfNoPreferredApp) {
  FakePublisher publisher(AppServiceProxyFactory::GetForProfile(&profile_));
  // Do not mark this app as preferred. The browser should be opened instead.
  publisher.PublishAppWithUrlScope("abc", GURL("https://www.example.com/"));

  EXPECT_CALL(
      new_window_delegate(),
      OpenUrl(GURL("https://www.example.com/foo/"), testing::_, testing::_));

  MaybeLaunchPreferredAppForUrl(&profile_, GURL("https://www.example.com/foo/"),
                                LaunchSource::kFromTest);
}

TEST_F(LaunchUtilsNewWindowTest, LaunchUrlInInstalledAppOrBrowser_OneApp) {
  FakePublisher publisher(AppServiceProxyFactory::GetForProfile(&profile_));
  publisher.PublishAppWithUrlScope("abc", GURL("https://www.example.com/"));

  LaunchUrlInInstalledAppOrBrowser(
      &profile_, GURL("https://www.example.com/foo"), LaunchSource::kFromTest);

  ASSERT_EQ(publisher.last_launched_app(), "abc");
}

TEST_F(LaunchUtilsNewWindowTest,
       LaunchUrlInInstalledAppOrBrowser_TwoAppsNoPreferred) {
  FakePublisher publisher(AppServiceProxyFactory::GetForProfile(&profile_));

  publisher.PublishAppWithUrlScope("abc", GURL("https://www.example.com/"));
  publisher.PublishAppWithUrlScope("def", GURL("https://www.example.com/app/"));

  EXPECT_CALL(
      new_window_delegate(),
      OpenUrl(GURL("https://www.example.com/app/"), testing::_, testing::_));

  LaunchUrlInInstalledAppOrBrowser(
      &profile_, GURL("https://www.example.com/app/"), LaunchSource::kFromTest);
}

TEST_F(LaunchUtilsNewWindowTest,
       LaunchUrlInInstalledAppOrBrowser_MultipleApps_LaunchesPreferred) {
  FakePublisher publisher(AppServiceProxyFactory::GetForProfile(&profile_));

  publisher.PublishAppWithUrlScope("abc", GURL("https://www.example.com/"));
  publisher.PublishAppWithUrlScope("def", GURL("https://www.example.com/app/"));

  // Enable the link capturing preference for one of the apps. This app should
  // be launched by `LaunchUrlInInstalledAppOrBrowser`.
  ASSERT_EQ(test::EnableLinkCapturingByUser(&profile_, "def"), base::ok());

  LaunchUrlInInstalledAppOrBrowser(
      &profile_, GURL("https://www.example.com/app/"), LaunchSource::kFromTest);

  ASSERT_EQ(publisher.last_launched_app(), "def");
}

TEST_F(LaunchUtilsNewWindowTest,
       LaunchUrlInInstalledAppOrBrowser_NoApp_LaunchesInBrowser) {
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("https://www.example.com"), testing::_, testing::_));

  LaunchUrlInInstalledAppOrBrowser(&profile_, GURL("https://www.example.com/"),
                                   LaunchSource::kFromTest);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace apps
