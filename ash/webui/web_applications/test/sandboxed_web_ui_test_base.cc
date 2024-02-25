// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/memory/raw_ptr.h"

#include <vector>

#include "ash/webui/common/trusted_types_test_util.h"
#include "ash/webui/web_applications/webui_test_prod_util.h"
#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/test/base/ash/js_test_api.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void HandleTestFileRequestCallback(
    const base::FilePath& test_file_location,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath source_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root);
  base::FilePath test_file_path =
      source_root.Append(test_file_location).AppendASCII(path);
  // First try DIR_SRC_TEST_DATA_ROOT, then generated files.
  if (!base::PathExists(test_file_path)) {
    base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &source_root);
    test_file_path = source_root.Append(test_file_location).AppendASCII(path);
  }
  if (!base::PathExists(test_file_path)) {
    LOG(ERROR) << "Test file not found in source tree or generated files: "
               << test_file_path;
  }

  std::string contents;
  CHECK(base::ReadFileToString(test_file_path, &contents)) << test_file_path;

  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(contents)));
}

bool TestRequestHandlerShouldHandleRequest(
    const std::vector<std::string>& test_paths,
    const std::string& path) {
  return base::Contains(test_paths, path);
}

std::string DefaultScriptTimeoutLog(const std::string& script,
                                    const base::TimeDelta& timeout) {
  return base::StringPrintf("Hit timeout of %fs:\n", timeout.InSecondsF()) +
         script;
}

// Loads a JS module into `target`, avoiding "Cannot use import statement
// outside a module" errors from direct injection. Uses the
// `ash-webui-test-script` TrustedTypes policy.
void MaybeLoadTestModule(const content::ToRenderFrameHost& target,
                         const std::string& resource) {
  if (resource.empty()) {
    return;
  }

  ASSERT_TRUE(ash::test_util::AddTestStaticUrlPolicy(target));
  constexpr char kScript[] = R"(
      (() => {
        const s = document.createElement('script');
        s.type = 'module';
        s.src = window.testStaticUrlPolicy.createScriptURL('$1');
        document.body.appendChild(s);
      })();
  )";
  ASSERT_TRUE(content::ExecJs(
      target, base::ReplaceStringPlaceholders(kScript, {resource}, nullptr)));
}

// Loads the test harness JS module into `target`. Uses the `test-harness`
// TrustedTypes policy. Waits for the module to be loaded.
void LoadTestHarnessModule(const content::ToRenderFrameHost& target,
                           const std::string& resource) {
  constexpr char kScript[] = R"(
      (async function LoadTestHarnessModule() {
        const testHarnessPolicy = trustedTypes.createPolicy('test-harness', {
          createScriptURL: () => './$1',
        });
        const tests = document.createElement('script');
        tests.type = 'module';
        tests.src = testHarnessPolicy.createScriptURL('');
        await new Promise((resolve, reject) => {
          tests.onload = resolve;
          tests.onerror = reject;
          document.body.appendChild(tests);
        });
      })();
  )";
  ASSERT_TRUE(content::ExecJs(
      target, base::ReplaceStringPlaceholders(kScript, {resource}, nullptr)));
}

}  // namespace

class SandboxedWebUiAppTestBase::TestCodeInjector
    : public content::TestNavigationObserver {
 public:
  explicit TestCodeInjector(SandboxedWebUiAppTestBase* owner)
      : TestNavigationObserver(GURL(owner->host_url_)), owner_(owner) {
    WatchExistingWebContents();
    StartWatchingNewWebContents();
  }

  // TestNavigationObserver:
  void OnDidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL().DeprecatedGetOriginAsURL() !=
        GURL(owner_->sandboxed_url_))
      return;

    auto* guest_frame = navigation_handle->GetRenderFrameHost();

    // Inject the JS Test API first (assertEquals, etc).
    std::vector<base::FilePath> scripts = JsTestApiConfig().default_libraries;
    scripts.insert(scripts.end(), owner_->scripts_.begin(),
                   owner_->scripts_.end());

    for (const auto& script : scripts) {
      ASSERT_TRUE(content::ExecJs(guest_frame, LoadJsTestLibrary(script)));
    }
    MaybeLoadTestModule(guest_frame, owner_->guest_test_module_);
    TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
  }

 private:
  const raw_ptr<SandboxedWebUiAppTestBase> owner_;
};

SandboxedWebUiAppTestBase::SandboxedWebUiAppTestBase(
    const std::string& host_url,
    const std::string& sandboxed_url,
    const std::vector<base::FilePath>& scripts,
    const std::string& guest_test_module,
    const std::string& test_harness_module)
    : host_url_(host_url),
      sandboxed_url_(sandboxed_url),
      scripts_(scripts),
      guest_test_module_(guest_test_module),
      test_harness_module_(test_harness_module) {}

SandboxedWebUiAppTestBase::~SandboxedWebUiAppTestBase() = default;

void SandboxedWebUiAppTestBase::RunCurrentTest(const std::string& helper) {
  const testing::TestInfo* test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  const std::string fixture = test_info->test_suite_name();
  const std::string name = test_info->name();
  LOG(INFO) << "Navigating to " << host_url_ << " to run " << fixture << "."
            << name << " from " << test_harness_module_;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(host_url_)));

  for (const auto& script : JsTestApiConfig().default_libraries) {
    ASSERT_TRUE(content::ExecJs(web_contents, LoadJsTestLibrary(script)));
  }
  LoadTestHarnessModule(web_contents, test_harness_module_);

  constexpr char kScript[] = "isolatedTestRunner('$1', '$2', '$3')";
  const std::string test_runner_script = base::ReplaceStringPlaceholders(
      kScript, {fixture, name, helper}, nullptr);
  EXPECT_EQ("test_completed", content::EvalJs(web_contents, test_runner_script))
      << test_runner_script;
  EXPECT_TRUE(EnsureNoCapturedConsoleErrorMessages());
}

// static
void SandboxedWebUiAppTestBase::ConfigureDefaultTestRequestHandler(
    const base::FilePath& root_folder,
    const std::vector<std::string>& resource_files) {
  SetTestableDataSourceRequestHandlerForTesting(
      base::BindRepeating(&TestRequestHandlerShouldHandleRequest,
                          resource_files),
      base::BindRepeating(&HandleTestFileRequestCallback, root_folder));
}

// static
std::string SandboxedWebUiAppTestBase::LoadJsTestLibrary(
    const base::FilePath& script_path) {
  base::FilePath source_root;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));
  const auto full_script_path =
      script_path.IsAbsolute() ? script_path : source_root.Append(script_path);

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string injected_content;
  EXPECT_TRUE(base::ReadFileToString(full_script_path, &injected_content));
  return injected_content;
}

// static
content::RenderFrameHost* SandboxedWebUiAppTestBase::GetAppFrame(
    content::WebContents* web_ui) {
  // Assume the first subframe is the app.
  content::RenderFrameHost* subframe = ChildFrameAt(web_ui, 0);
  EXPECT_TRUE(subframe) << web_ui->GetVisibleURL();
  return subframe;
}

// static
content::EvalJsResult SandboxedWebUiAppTestBase::EvalJsInAppFrame(
    content::WebContents* web_ui,
    const std::string& script) {
  // Clients of this helper all run in the same isolated world.
  constexpr int kWorldId = 1;

  base::TimeDelta script_timeout = TestTimeouts::action_timeout();
  base::test::ScopedRunLoopTimeout scoped_run_timeout(
      FROM_HERE, script_timeout,
      base::BindRepeating(&DefaultScriptTimeoutLog, script, script_timeout));

  return EvalJs(GetAppFrame(web_ui), script,
                content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, kWorldId);
}

void SandboxedWebUiAppTestBase::SetUpOnMainThread() {
  injector_ = std::make_unique<TestCodeInjector>(this);
  MojoWebUIBrowserTest::SetUpOnMainThread();
}
