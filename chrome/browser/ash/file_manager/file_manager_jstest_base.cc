// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_jstest_base.h"

#include "ash/webui/file_manager/resource_loader.h"
#include "ash/webui/file_manager/resources/grit/file_manager_swa_resources_map.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/file_manager/grit/file_manager_gen_resources_map.h"
#include "ui/file_manager/grit/file_manager_resources_map.h"

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
    // Add a data source to serve all the chrome://file-manager files for Image
    // Loader.
    auto* profile = Profile::FromWebUI(web_ui);
    content::WebUIDataSource* files_swa_source =
        content::WebUIDataSource::Create(
            ash::file_manager::kChromeUIFileManagerHost);

    files_swa_source->AddResourcePaths(base::make_span(
        kFileManagerSwaResources, kFileManagerSwaResourcesSize));

    ash::file_manager::AddFilesAppResources(
        files_swa_source, kFileManagerResources, kFileManagerResourcesSize);
    ash::file_manager::AddFilesAppResources(files_swa_source,
                                            kFileManagerGenResources,
                                            kFileManagerGenResourcesSize);

    dict_ = GetFileManagerStrings();
    AddFileManagerFeatureStrings("en-US", Profile::FromWebUI(web_ui), &dict_);
    files_swa_source->AddLocalizedStrings(dict_);
    files_swa_source->UseStringsJs();

    content::WebUIDataSource::Add(profile, files_swa_source);

    return std::make_unique<content::WebUIController>(web_ui);
  }

  void DataSourceOverrides(content::WebUIDataSource* source) override {
    // Add 'unsafe-inline' to CSP to allow the inline <script> in the
    // generated HTML to run see js_test_gen_html.py.
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc,
        "script-src chrome://resources chrome://webui-test " +
            std::string(ash::file_manager::kChromeUIFileManagerURL) +
            " "
            "'self' chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj "
            "chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp ; ");

    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrcElem,
        "script-src chrome://resources chrome://webui-test " +
            std::string(ash::file_manager::kChromeUIFileManagerURL) +
            " "
            "'self' chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj "
            "chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp ; ");

    // TODO(crbug.com/1098685): Trusted Type remaining WebUI.
    source->DisableTrustedTypesCSP();

    DCHECK(!dict_.empty()) << "The translation should be fully loaded";
    source->AddLocalizedStrings(dict_);
    source->UseStringsJs();
  }

 private:
  base::Value::Dict dict_;
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

  // The test might have finished loading.
  bool is_test_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "domAutomationController.send(window.__TEST_LOADED__);",
      &is_test_loaded));

  if (!is_test_loaded) {
    // Wait for the JS modules to be loaded and exported to window.
    content::DOMMessageQueue message_queue(web_contents);
    std::string message;
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ(message, "\"LOADED\"");
  }

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
