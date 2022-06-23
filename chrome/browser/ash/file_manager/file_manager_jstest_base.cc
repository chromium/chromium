// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_jstest_base.h"

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/test_data_source.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "net/base/filename_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// WebUIProvider to attach the URLDataSource for the test URL during tests.
// Used to start the unittest from a chrome:// URL which allows unittest files
// (HTML/JS/CSS) to load other resources from WebUI URLs chrome://*.
class TestWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  TestWebUIProvider() = default;

  TestWebUIProvider(const TestWebUIProvider&) = delete;
  TestWebUIProvider& operator=(const TestWebUIProvider&) = delete;

  ~TestWebUIProvider() override = default;

  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override {
    return std::make_unique<content::WebUIController>(web_ui);
  }

  void DataSourceOverrides(content::WebUIDataSource* source) override {
    // Add 'unsafe-inline' to CSP to allow the inline <script> in the
    // generated HTML to run see js_test_gen_html.py.
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc,
        "script-src chrome://resources chrome://webui-test "
        "'self' chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj "
        "chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp ");

    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrcElem,
        "script-src chrome://resources chrome://webui-test "
        "'self' chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj "
        "chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp ; ");

    // TODO(crbug.com/1098685): Trusted Type remaining WebUI.
    source->DisableTrustedTypesCSP();
  }
};

base::LazyInstance<TestWebUIProvider>::DestructorAtExit test_webui_provider_ =
    LAZY_INSTANCE_INITIALIZER;

static const GURL TestResourceUrl() {
  static GURL url(content::GetWebUIURLString("webui-test"));
  return url;
}

}  // namespace

FileManagerJsTestBase::FileManagerJsTestBase(const base::FilePath& base_path)
    : base_path_(base_path) {}

FileManagerJsTestBase::~FileManagerJsTestBase() {}

void FileManagerJsTestBase::RunTestURL(const std::string& file) {
  // Open a new tab with the Files app test harness.
  auto url =
      GURL("chrome://webui-test/base/js/test_harness.html?test_module=/" +
           base_path_.Append(file).value());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Wait for the JS modules to be loaded and exported to window.
  content::DOMMessageQueue message_queue(web_contents);
  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ(message, "\"LOADED\"");

  // Execute the WebUI test harness.
  EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents));
}

void FileManagerJsTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  base::FilePath pak_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &pak_path));
  pak_path = pak_path.AppendASCII("browser_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path, ui::kScaleFactorNone);

  webui_controller_factory_ =
      std::make_unique<TestChromeWebUIControllerFactory>();
  webui_controller_factory_registration_ =
      std::make_unique<content::ScopedWebUIControllerFactoryRegistration>(
          webui_controller_factory_.get(),
          ChromeWebUIControllerFactory::GetInstance());
  webui_controller_factory_->AddFactoryOverride(TestResourceUrl().host(),
                                                test_webui_provider_.Pointer());
  Profile* profile = browser()->profile();
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile);
}

void FileManagerJsTestBase::TearDownOnMainThread() {
  InProcessBrowserTest::TearDownOnMainThread();

  webui_controller_factory_->RemoveFactoryOverride(TestResourceUrl().host());
}
