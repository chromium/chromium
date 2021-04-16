// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/test_data_source.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

base::FilePath GetGenRoot() {
  base::FilePath executable_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &executable_path));
  return executable_path.AppendASCII("gen");
}

// URLDataSource for the test URL chrome://file_manager_test/. It reads files
// directly from repository source.
class TestFilesDataSource : public content::URLDataSource {
 public:
  TestFilesDataSource() {}
  ~TestFilesDataSource() override {}

 private:
  // This has to match TestResourceUrl()
  std::string GetSource() override { return "file_manager_test"; }

  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override {
    const std::string path = content::URLDataSource::URLToRequestPath(url);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&TestFilesDataSource::ReadFile, base::Unretained(this),
                       path, std::move(callback)));
  }

  void ReadFile(const std::string& path,
                content::URLDataSource::GotDataCallback callback) {
    if (source_root_.empty()) {
      CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_));
    }
    if (gen_root_.empty()) {
      CHECK(base::PathService::Get(base::DIR_EXE, &gen_root_));
      gen_root_ = GetGenRoot();
    }

    std::string content;

    base::FilePath src_file_path =
        source_root_.Append(base::FilePath::FromUTF8Unsafe(path));
    base::FilePath gen_file_path =
        gen_root_.Append(base::FilePath::FromUTF8Unsafe(path));

    // Do some basic validation of the file extension.
    CHECK(src_file_path.Extension() == ".html" ||
          src_file_path.Extension() == ".js" ||
          src_file_path.Extension() == ".css" ||
          src_file_path.Extension() == ".svg")
        << "chrome://file_manager_test/ only supports .html/.js/.css/.svg "
           "extension files";

    CHECK(base::PathExists(src_file_path) || base::PathExists(gen_file_path))
        << src_file_path << " or: " << gen_file_path << " input path: " << path;
    CHECK(base::ReadFileToString(src_file_path, &content) ||
          base::ReadFileToString(gen_file_path, &content))
        << src_file_path << " or: " << gen_file_path;

    scoped_refptr<base::RefCountedString> response =
        base::RefCountedString::TakeString(&content);
    std::move(callback).Run(response.get());
  }

  bool ShouldServeMimeTypeAsContentTypeHeader() override { return true; }

  // It currently only serves HTML/JS/CSS/SVG.
  std::string GetMimeType(const std::string& path) override {
    if (base::EndsWith(path, ".html", base::CompareCase::INSENSITIVE_ASCII)) {
      return "text/html";
    }

    if (base::EndsWith(path, ".css", base::CompareCase::INSENSITIVE_ASCII)) {
      return "text/css";
    }

    if (base::EndsWith(path, ".js", base::CompareCase::INSENSITIVE_ASCII)) {
      return "application/javascript";
    }

    if (base::EndsWith(path, ".svg", base::CompareCase::INSENSITIVE_ASCII)) {
      return "image/svg+xml";
    }

    LOG(FATAL) << "unsupported file type: " << path;
    return {};
  }

  std::string GetContentSecurityPolicy(
      const network::mojom::CSPDirectiveName directive) override {
    if (directive == network::mojom::CSPDirectiveName::ScriptSrc) {
      // Add 'unsafe-inline' to CSP to allow the inline <script> in the
      // generated HTML to run see js_test_gen_html.py.
      return "script-src chrome://resources chrome://test 'self'  "
             "chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj "
             "chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp "
             "'unsafe-inline'; ";
    } else if (directive ==
                   network::mojom::CSPDirectiveName::RequireTrustedTypesFor ||
               directive == network::mojom::CSPDirectiveName::TrustedTypes) {
      return std::string();
    }

    return content::URLDataSource::GetContentSecurityPolicy(directive);
  }

  // Root of repository source, where files are served directly from.
  base::FilePath source_root_;
  base::FilePath gen_root_;

  DISALLOW_COPY_AND_ASSIGN(TestFilesDataSource);
};

// WebUIProvider to attach the URLDataSource for the test URL during tests.
// Used to start the unittest from a chrome:// URL which allows unittest files
// (HTML/JS/CSS) to load other resources from WebUI URLs chrome://*.
class TestWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  TestWebUIProvider() = default;
  ~TestWebUIProvider() override = default;

  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override {
    auto* profile = Profile::FromWebUI(web_ui);
    content::URLDataSource::Add(profile,
                                std::make_unique<TestFilesDataSource>());
    content::URLDataSource::Add(profile,
                                std::make_unique<TestDataSource>("webui"));

    return std::make_unique<content::WebUIController>(web_ui);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIProvider);
};

base::LazyInstance<TestWebUIProvider>::DestructorAtExit test_webui_provider_ =
    LAZY_INSTANCE_INITIALIZER;

static const GURL TestResourceUrl() {
  static GURL url(content::GetWebUIURLString("file_manager_test"));
  return url;
}

}  // namespace

FileManagerJsTestBase::FileManagerJsTestBase(const base::FilePath& base_path)
    : base_path_(base_path) {}

FileManagerJsTestBase::~FileManagerJsTestBase() {}

void FileManagerJsTestBase::RunTest(const base::FilePath& file) {
  base::FilePath root_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path));
  base::FilePath full_path = root_path.Append(base_path_).Append(file);
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::PathExists(full_path)) << full_path.value();
  }
  RunTestImpl(net::FilePathToFileURL(full_path));
}

void FileManagerJsTestBase::RunGeneratedTest(const std::string& file) {
  base::FilePath path = GetGenRoot();

  // Serve the generated html file from out/gen. It references files from
  // DIR_SOURCE_ROOT, so serve from there as well. An alternative would be to
  // copy the js files as a build step and serve file:// URLs, but the embedded
  // test server gives better output for troubleshooting errors.
  embedded_test_server()->ServeFilesFromDirectory(path.Append(base_path_));
  embedded_test_server()->ServeFilesFromSourceDirectory(base::FilePath());

  ASSERT_TRUE(embedded_test_server()->Start());
  RunTestImpl(embedded_test_server()->GetURL(file));
}

void FileManagerJsTestBase::RunTestURL(const std::string& file) {
  RunTestImpl(
      GURL("chrome://file_manager_test/" + base_path_.Append(file).value()));
}

void FileManagerJsTestBase::RunTestImpl(const GURL& url) {
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents, {}));
}

void FileManagerJsTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();

  content::WebUIControllerFactory::UnregisterFactoryForTesting(
      ChromeWebUIControllerFactory::GetInstance());

  webui_controller_factory_ =
      std::make_unique<TestChromeWebUIControllerFactory>();
  content::WebUIControllerFactory::RegisterFactory(
      webui_controller_factory_.get());
  webui_controller_factory_->AddFactoryOverride(TestResourceUrl().host(),
                                                test_webui_provider_.Pointer());
  Profile* profile = browser()->profile();
  file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile);
}

void FileManagerJsTestBase::TearDownOnMainThread() {
  InProcessBrowserTest::TearDownOnMainThread();

  webui_controller_factory_->RemoveFactoryOverride(TestResourceUrl().host());
  content::WebUIControllerFactory::UnregisterFactoryForTesting(
      webui_controller_factory_.get());

  // This is needed to avoid a debug assert after the test completes, see stack
  // trace in http://crrev.com/179347
  content::WebUIControllerFactory::RegisterFactory(
      ChromeWebUIControllerFactory::GetInstance());

  webui_controller_factory_.reset();
}
