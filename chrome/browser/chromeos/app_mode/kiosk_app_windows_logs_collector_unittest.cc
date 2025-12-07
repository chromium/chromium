// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_windows_logs_collector.h"

#include <memory>

#include "ash/test/ash_test_helper.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_saver.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/manifest.mojom-data-view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const auto* kDefaultMessage = u"This is the default message 1.";
const auto* kDefaultMessage2 = u"This is the default message 2.";
const auto* kDefaultMessage3 = u"This is the default message 3.";
const auto* kDefaultSource = u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr auto* kDefaultSource2 = u"aaaabbbbaaaabbbbaaaabbbbaaaabbbb";
constexpr auto* kDefaultSource3 = u"aaaaccccaaaaccccaaaaccccaaaacccc";
constexpr int kDefaultLineNumber = 0;
constexpr int kDefaultLineNumber2 = 100;
constexpr int kDefaultLineNumber3 = 200;
constexpr char kLaunchURL[] = "https://foo.example/";

scoped_refptr<extensions::Extension> AddChromeApp(const std::string& app_id) {
  base::Value::Dict manifest;
  manifest.SetByDottedPath(extensions::manifest_keys::kName,
                           "Kiosk app windows logs test.");
  manifest.SetByDottedPath(extensions::manifest_keys::kVersion, "1");
  manifest.SetByDottedPath(extensions::manifest_keys::kManifestVersion, 2);
  manifest.SetByDottedPath(extensions::manifest_keys::kDescription,
                           "for testing pinned apps");
  // AppService checks the app's type. So set the
  // manifest_keys::kLaunchWebURL, so that the extension can get the type
  // from manifest value, and then AppService can get the extension's type.
  manifest.SetByDottedPath(extensions::manifest_keys::kLaunchWebURL,
                           kLaunchURL);

  std::u16string error;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
          manifest, extensions::Extension::NO_FLAGS, app_id, &error);
  CHECK(error.empty()) << error;
  return extension;
}

void InitAppWindow(extensions::AppWindow* app_window, const gfx::Rect& bounds) {
  // Create a TestAppWindowContents for the ShellAppDelegate to initialize the
  // ShellExtensionWebContentsObserver with.
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(
          app_window->browser_context(), /*site_instance=*/nullptr));
  auto app_window_contents =
      std::make_unique<extensions::TestAppWindowContents>(
          std::move(web_contents));

  // Initialize the web contents and AppWindow.
  app_window->app_delegate()->InitWebContents(
      app_window_contents->GetWebContents());

  content::RenderFrameHost* main_frame =
      app_window_contents->GetWebContents()->GetPrimaryMainFrame();
  DCHECK(main_frame);

  extensions::AppWindow::CreateParams params;
  params.content_spec.bounds = bounds;
  app_window->Init(GURL(), std::move(app_window_contents), main_frame, params);
}

extensions::AppWindow* CreateAppWindowForApp(
    Profile* profile,
    const extensions::Extension* extension,
    gfx::Rect bounds = {}) {
  extensions::AppWindow* app_window = new extensions::AppWindow(
      profile, std::make_unique<ChromeAppDelegate>(profile, true), extension);
  InitAppWindow(app_window, bounds);
  return app_window;
}

}  // namespace

class KioskAppWindowsLogsCollectorTest
    : public extensions::ExtensionServiceTestBase {
 public:
  KioskAppWindowsLogsCollectorTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                content::BrowserTaskEnvironment::REAL_IO_THREAD)) {}

  void SetUp() override {
    ash_test_helper_.SetUp(ash::AshTestHelper::InitParams());

    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kForceAppMode);
    command_line_.GetProcessCommandLine()->AppendSwitch(switches::kAppId);

    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    apps::WaitForAppServiceProxyReady(
        apps::AppServiceProxyFactory::GetForProfile(profile()));

    chrome_app_ = AddChromeApp(kAppId);
    registrar()->AddExtension(chrome_app_.get());
  }

  void TearDown() override {
    logs_collector_.reset();
    extensions::ExtensionServiceTestBase::TearDown();
    ash_test_helper_.TearDown();
  }

  void CreateLogsCollector(
      KioskWebContentsObserver::LoggerCallback logger_callback) {
    logs_collector_ = std::make_unique<KioskAppWindowsLogsCollector>(
        profile(), std::move(logger_callback));
  }

  void AddMessageToConsole(
      content::WebContents* web_contents,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) {
    auto* tester = content::WebContentsTester::For(web_contents);
    tester->TestDidAddMessageToConsole(log_level, message, line_no, source_id,
                                       untrusted_stack_trace);
  }

  extensions::AppWindow* CreateAppWindow() {
    extensions::AppWindow* app_window =
        CreateAppWindowForApp(profile(), chrome_app_.get());
    CHECK(app_window);
    CHECK(app_window->web_contents());

    return app_window;
  }

 private:
  ash::AshTestHelper ash_test_helper_;
  base::test::ScopedCommandLine command_line_;

  scoped_refptr<extensions::Extension> chrome_app_;
  std::unique_ptr<KioskAppWindowsLogsCollector> logs_collector_;
};

TEST_F(KioskAppWindowsLogsCollectorTest, ShouldCollectLogsFromAppWindow) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;
  CreateLogsCollector(result_future.GetCallback());

  auto* app_window1 = CreateAppWindow();
  AddMessageToConsole(app_window1->web_contents(),
                      blink::mojom::ConsoleMessageLevel::kInfo, kDefaultMessage,
                      kDefaultLineNumber, kDefaultSource, std::nullopt);

  auto log = result_future.Take();
  EXPECT_EQ(log.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log.message(), kDefaultMessage);
  EXPECT_EQ(log.source(), kDefaultSource);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kInfo);
}

TEST_F(KioskAppWindowsLogsCollectorTest,
       ShouldCollectLogsFromMultipleAppWindows) {
  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;
  CreateLogsCollector(result_future.GetCallback());

  auto* app_window1 = CreateAppWindow();
  AddMessageToConsole(app_window1->web_contents(),
                      blink::mojom::ConsoleMessageLevel::kInfo, kDefaultMessage,
                      kDefaultLineNumber, kDefaultSource, std::nullopt);

  auto log = result_future.Take();
  EXPECT_EQ(log.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log.message(), kDefaultMessage);
  EXPECT_EQ(log.source(), kDefaultSource);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kInfo);

  auto* app_window2 = CreateAppWindow();
  AddMessageToConsole(
      app_window2->web_contents(), blink::mojom::ConsoleMessageLevel::kError,
      kDefaultMessage2, kDefaultLineNumber2, kDefaultSource2, std::nullopt);

  auto log2 = result_future.Take();
  EXPECT_EQ(log2.line_no(), kDefaultLineNumber2);
  EXPECT_EQ(log2.message(), kDefaultMessage2);
  EXPECT_EQ(log2.source(), kDefaultSource2);
  EXPECT_EQ(log2.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log2.severity(), blink::mojom::ConsoleMessageLevel::kError);

  auto* app_window3 = CreateAppWindow();
  AddMessageToConsole(
      app_window3->web_contents(), blink::mojom::ConsoleMessageLevel::kWarning,
      kDefaultMessage3, kDefaultLineNumber3, kDefaultSource3, std::nullopt);

  auto log3 = result_future.Take();
  EXPECT_EQ(log3.line_no(), kDefaultLineNumber3);
  EXPECT_EQ(log3.message(), kDefaultMessage3);
  EXPECT_EQ(log3.source(), kDefaultSource3);
  EXPECT_EQ(log3.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log3.severity(), blink::mojom::ConsoleMessageLevel::kWarning);
}

TEST_F(KioskAppWindowsLogsCollectorTest,
       ShouldCollectLogsFromExistingAppWindows) {
  auto* app_window1 = CreateAppWindow();
  auto* app_window2 = CreateAppWindow();
  auto* app_window3 = CreateAppWindow();

  base::test::RepeatingTestFuture<
      const KioskAppLevelLogsSaver::KioskLogMessage&>
      result_future;
  CreateLogsCollector(result_future.GetCallback());

  AddMessageToConsole(app_window1->web_contents(),
                      blink::mojom::ConsoleMessageLevel::kInfo, kDefaultMessage,
                      kDefaultLineNumber, kDefaultSource, std::nullopt);

  auto log = result_future.Take();
  EXPECT_EQ(log.line_no(), kDefaultLineNumber);
  EXPECT_EQ(log.message(), kDefaultMessage);
  EXPECT_EQ(log.source(), kDefaultSource);
  EXPECT_EQ(log.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log.severity(), blink::mojom::ConsoleMessageLevel::kInfo);

  AddMessageToConsole(
      app_window2->web_contents(), blink::mojom::ConsoleMessageLevel::kError,
      kDefaultMessage2, kDefaultLineNumber2, kDefaultSource2, std::nullopt);

  auto log2 = result_future.Take();
  EXPECT_EQ(log2.line_no(), kDefaultLineNumber2);
  EXPECT_EQ(log2.message(), kDefaultMessage2);
  EXPECT_EQ(log2.source(), kDefaultSource2);
  EXPECT_EQ(log2.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log2.severity(), blink::mojom::ConsoleMessageLevel::kError);

  AddMessageToConsole(
      app_window3->web_contents(), blink::mojom::ConsoleMessageLevel::kWarning,
      kDefaultMessage3, kDefaultLineNumber3, kDefaultSource3, std::nullopt);

  auto log3 = result_future.Take();
  EXPECT_EQ(log3.line_no(), kDefaultLineNumber3);
  EXPECT_EQ(log3.message(), kDefaultMessage3);
  EXPECT_EQ(log3.source(), kDefaultSource3);
  EXPECT_EQ(log3.untrusted_stack_trace(), std::nullopt);
  EXPECT_EQ(log3.severity(), blink::mojom::ConsoleMessageLevel::kWarning);
}

}  // namespace chromeos
