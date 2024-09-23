// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// A helper object to wait for and collect the results from a script execution.
class ScriptExecutorHelper {
 public:
  ScriptExecutorHelper() = default;
  ScriptExecutorHelper(const ScriptExecutorHelper&) = delete;
  ScriptExecutorHelper& operator=(const ScriptExecutorHelper&) = delete;
  ~ScriptExecutorHelper() = default;

  void Wait() { run_loop_.Run(); }

  ScriptExecutor::ScriptFinishedCallback GetCallback() {
    // The Unretained() is safe because this object is always supposed to
    // outlive the script execution.
    return base::BindOnce(&ScriptExecutorHelper::OnScriptFinished,
                          base::Unretained(this));
  }

  const std::vector<ScriptExecutor::FrameResult>& results() const {
    return results_;
  }

 private:
  void OnScriptFinished(
      std::vector<ScriptExecutor::FrameResult> frame_results) {
    results_ = std::move(frame_results);
    run_loop_.Quit();
  }

  std::vector<ScriptExecutor::FrameResult> results_;
  base::RunLoop run_loop_;
};

}  // namespace

class ScriptExecutorBrowserTest : public ExtensionBrowserTest {
 public:
  ScriptExecutorBrowserTest() = default;
  ScriptExecutorBrowserTest(const ScriptExecutorBrowserTest&) = delete;
  ScriptExecutorBrowserTest& operator=(const ScriptExecutorBrowserTest&) =
      delete;
  ~ScriptExecutorBrowserTest() override = default;

  const Extension* LoadExtensionWithHostPermission(
      const std::string& host_permission) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("extension")
            .AddHostPermission(host_permission)
            .Build();
    extension_service()->AddExtension(extension.get());
    EXPECT_TRUE(
        extension_registry()->enabled_extensions().GetByID(extension->id()));
    return extension.get();
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Returns the frame with the given `name` from `web_contents`.
  content::RenderFrameHost* GetFrameByName(content::WebContents* web_contents,
                                           const std::string& name) {
    return content::FrameMatchingPredicate(
        web_contents->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, name));
  }
};

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, MainWorldExecution) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  {
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_com));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  constexpr char kSetFlagScript[] = "window.mainWorldFlag = 'executionFlag';";
  // NOTE: We *need* this to happen in the main world for the test.
  EXPECT_TRUE(content::ExecJs(main_frame, kSetFlagScript));

  ScriptExecutor script_executor(web_contents);

  ScriptExecutorHelper helper;
  std::vector<mojom::JSSourcePtr> sources;
  sources.push_back(mojom::JSSource::New("window.mainWorldFlag", GURL()));
  script_executor.ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
      mojom::CodeInjection::NewJs(mojom::JSInjection::New(
          std::move(sources), mojom::ExecutionWorld::kMain,
          /*world_id=*/std::nullopt,
          blink::mojom::WantResultOption::kWantResult,
          blink::mojom::UserActivationOption::kDoNotActivate,
          blink::mojom::PromiseResultOption::kAwait)),
      ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
      ScriptExecutor::DONT_MATCH_ABOUT_BLANK, mojom::RunLocation::kDocumentIdle,
      ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
      helper.GetCallback());
  helper.Wait();

  ASSERT_EQ(1u, helper.results().size());
  EXPECT_EQ(web_contents->GetLastCommittedURL(), helper.results()[0].url);
  EXPECT_EQ(base::Value("executionFlag"), helper.results()[0].value);
  EXPECT_EQ(0, helper.results()[0].frame_id);
  EXPECT_EQ("", helper.results()[0].error);
}

IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, MainFrameExecution) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  {
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_com));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  EXPECT_EQ("OK", base::UTF16ToUTF8(web_contents->GetTitle()));

  ScriptExecutor script_executor(web_contents);
  constexpr char kCode[] =
      R"(let oldTitle = document.title;
         document.title = 'New Title';
         oldTitle;
        )";

  ScriptExecutorHelper helper;
  std::vector<mojom::JSSourcePtr> sources;
  sources.push_back(mojom::JSSource::New(kCode, GURL()));
  script_executor.ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
      mojom::CodeInjection::NewJs(mojom::JSInjection::New(
          std::move(sources), mojom::ExecutionWorld::kIsolated,
          /*world_id=*/std::nullopt,
          blink::mojom::WantResultOption::kWantResult,
          blink::mojom::UserActivationOption::kDoNotActivate,
          blink::mojom::PromiseResultOption::kAwait)),
      ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
      ScriptExecutor::DONT_MATCH_ABOUT_BLANK, mojom::RunLocation::kDocumentIdle,
      ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
      helper.GetCallback());
  helper.Wait();
  EXPECT_EQ("New Title", base::UTF16ToUTF8(web_contents->GetTitle()));

  ASSERT_EQ(1u, helper.results().size());
  EXPECT_EQ(web_contents->GetLastCommittedURL(), helper.results()[0].url);
  EXPECT_EQ(base::Value("OK"), helper.results()[0].value);
  EXPECT_EQ(0, helper.results()[0].frame_id);
  EXPECT_EQ("", helper.results()[0].error);
}

// Tests injecting multiple JS sources into a frame.
IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, MultipleSourceExecution) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  {
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_com));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  EXPECT_EQ("OK", base::UTF16ToUTF8(web_contents->GetTitle()));

  // Inject two pieces of code. Note that the second references a variable set
  // by the first, which thus also exercises injection order (in addition to
  // that they both run).
  ScriptExecutor script_executor(web_contents);
  constexpr char kCode1[] =
      R"(window.newTitle = 'New Title';
         'First Result';)";
  constexpr char kCode2[] =
      R"(document.title = window.newTitle;
         'Second Result';)";

  ScriptExecutorHelper helper;
  std::vector<mojom::JSSourcePtr> sources;
  sources.push_back(mojom::JSSource::New(kCode1, GURL()));
  sources.push_back(mojom::JSSource::New(kCode2, GURL()));
  script_executor.ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
      mojom::CodeInjection::NewJs(mojom::JSInjection::New(
          std::move(sources), mojom::ExecutionWorld::kIsolated,
          /*world_id=*/std::nullopt,
          blink::mojom::WantResultOption::kWantResult,
          blink::mojom::UserActivationOption::kDoNotActivate,
          blink::mojom::PromiseResultOption::kAwait)),
      ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
      ScriptExecutor::DONT_MATCH_ABOUT_BLANK, mojom::RunLocation::kDocumentIdle,
      ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
      helper.GetCallback());
  helper.Wait();
  EXPECT_EQ("New Title", base::UTF16ToUTF8(web_contents->GetTitle()));

  ASSERT_EQ(1u, helper.results().size());
  EXPECT_EQ(web_contents->GetLastCommittedURL(), helper.results()[0].url);
  EXPECT_EQ(base::Value("Second Result"), helper.results()[0].value);
  EXPECT_EQ(0, helper.results()[0].frame_id);
  EXPECT_EQ("", helper.results()[0].error);
}

// Tests that scripts that evaluate to promises can be properly waited upon.
IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, PromisesResolve) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  {
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_com));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  EXPECT_EQ("OK", base::UTF16ToUTF8(web_contents->GetTitle()));

  ScriptExecutor script_executor(web_contents);

  {
    // Inject two pieces of code. They each evaluate to a promise. The second,
    // `kCode2`, evaluates to a promise that resolves immediately, and then
    // asynchronously resovles the promise from the first, `kCode1`, which
    // changes the title of the page.
    // This guarantees that the renderer code properly waits for *all* results
    // to resolve, and not simply the last one.
    constexpr char kCode1[] =
        R"((new Promise((resolve) => {
              window.resolveFirstPromise = resolve;
           }).then(() => {
              document.title = 'New Title';
           }));)";
    constexpr char kCode2[] =
        R"((new Promise((resolve) => {
              resolve('Second Promise');
              setTimeout(window.resolveFirstPromise, 0);
           }));)";

    ScriptExecutorHelper helper;
    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(mojom::JSSource::New(kCode1, GURL()));
    sources.push_back(mojom::JSSource::New(kCode2, GURL()));
    script_executor.ExecuteScript(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
        mojom::CodeInjection::NewJs(mojom::JSInjection::New(
            std::move(sources), mojom::ExecutionWorld::kIsolated,
            /*world_id=*/std::nullopt,
            blink::mojom::WantResultOption::kWantResult,
            blink::mojom::UserActivationOption::kDoNotActivate,
            blink::mojom::PromiseResultOption::kAwait)),
        ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
        GURL() /* webview_src */, helper.GetCallback());
    helper.Wait();

    EXPECT_EQ("New Title", base::UTF16ToUTF8(web_contents->GetTitle()));
    ASSERT_EQ(1u, helper.results().size());
    EXPECT_EQ(web_contents->GetLastCommittedURL(), helper.results()[0].url);
    EXPECT_EQ(base::Value("Second Promise"), helper.results()[0].value);
    EXPECT_EQ(0, helper.results()[0].frame_id);
    EXPECT_EQ("", helper.results()[0].error);
  }

  {
    // Next, inject code that evaluates to a promise, but don't include the
    // "wait_for_promise" flag. The returned result should be the promise
    // itself, which then serializes to an empty object (`{}`).
    constexpr char kCode[] = R"((new Promise((r) => { r('hello'); }));)";

    ScriptExecutorHelper helper;
    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(mojom::JSSource::New(kCode, GURL()));
    script_executor.ExecuteScript(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
        mojom::CodeInjection::NewJs(mojom::JSInjection::New(
            std::move(sources), mojom::ExecutionWorld::kIsolated,
            /*world_id=*/std::nullopt,
            blink::mojom::WantResultOption::kWantResult,
            blink::mojom::UserActivationOption::kDoNotActivate,
            blink::mojom::PromiseResultOption::kDoNotWait)),
        ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
        GURL() /* webview_src */, helper.GetCallback());
    helper.Wait();

    ASSERT_EQ(1u, helper.results().size());
    EXPECT_EQ(web_contents->GetLastCommittedURL(), helper.results()[0].url);
    EXPECT_EQ(base::Value(base::Value::Type::DICT), helper.results()[0].value);
    EXPECT_EQ(0, helper.results()[0].frame_id);
    EXPECT_EQ("", helper.results()[0].error);
  }
}

// Tests script execution into a specified set of frames.
IN_PROC_BROWSER_TEST_F(ScriptExecutorBrowserTest, SpecifiedFrames) {
  const Extension* extension =
      LoadExtensionWithHostPermission("http://example.com/*");

  GURL example_com = embedded_test_server()->GetURL(
      "example.com", "/extensions/iframes/main.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  {
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), example_com));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  // Note: The frame hierarchy for main.html looks like:
  // main
  //   frame1
  //   frame2
  //     frame2_child
  //   frame3
  content::RenderFrameHost* frame1 = GetFrameByName(web_contents, "frame1");
  ASSERT_TRUE(frame1);
  const int frame1_id = ExtensionApiFrameIdMap::GetFrameId(frame1);
  const GURL frame1_url = frame1->GetLastCommittedURL();
  content::RenderFrameHost* frame2 = GetFrameByName(web_contents, "frame2");
  ASSERT_TRUE(frame2);
  const int frame2_id = ExtensionApiFrameIdMap::GetFrameId(frame2);
  const GURL frame2_url = frame2->GetLastCommittedURL();
  content::RenderFrameHost* frame3 = GetFrameByName(web_contents, "frame3");
  ASSERT_TRUE(frame3);
  content::RenderFrameHost* frame2_child =
      GetFrameByName(web_contents, "frame2_child");
  ASSERT_TRUE(frame2_child);
  const int frame2_child_id = ExtensionApiFrameIdMap::GetFrameId(frame2_child);
  const GURL frame2_child_url = frame2_child->GetLastCommittedURL();

  ScriptExecutor script_executor(web_contents);
  // Note: Since other tests verify the code's effects, here we just rely on the
  // execution result as an indication that it ran.
  constexpr char kCode[] = "document.title;";

  const base::Value frame1_result("Frame 1");
  const base::Value frame2_result("Frame 2");
  const base::Value frame2_child_result("Frame 2 Child");

  auto get_result_matcher = [](const base::Value& value, int frame_id,
                               const GURL& url, const std::string& error = "") {
    return ::testing::AllOf(
        ::testing::Field(&ScriptExecutor::FrameResult::value,
                         ::testing::Eq(std::cref(value))),
        ::testing::Field(&ScriptExecutor::FrameResult::frame_id, frame_id),
        ::testing::Field(&ScriptExecutor::FrameResult::url, url),
        ::testing::Field(&ScriptExecutor::FrameResult::error, error));
  };

  {
    // Execute in frames 1 and 2. These are the only frames for which we should
    // get a result.
    ScriptExecutorHelper helper;
    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(mojom::JSSource::New(kCode, GURL()));
    script_executor.ExecuteScript(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
        mojom::CodeInjection::NewJs(mojom::JSInjection::New(
            std::move(sources), mojom::ExecutionWorld::kIsolated,
            /*world_id=*/std::nullopt,
            blink::mojom::WantResultOption::kWantResult,
            blink::mojom::UserActivationOption::kDoNotActivate,
            blink::mojom::PromiseResultOption::kAwait)),
        ScriptExecutor::SPECIFIED_FRAMES, {frame1_id, frame2_id},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
        GURL() /* webview_src */, helper.GetCallback());
    helper.Wait();

    EXPECT_THAT(helper.results(),
                testing::UnorderedElementsAre(
                    get_result_matcher(frame1_result, frame1_id, frame1_url),
                    get_result_matcher(frame2_result, frame2_id, frame2_url)));
  }

  {
    // Repeat the execution in frames 1 and 2, but include subframes. This
    // should result in frame2_child being added to the results.
    ScriptExecutorHelper helper;
    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(mojom::JSSource::New(kCode, GURL()));
    script_executor.ExecuteScript(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
        mojom::CodeInjection::NewJs(mojom::JSInjection::New(
            std::move(sources), mojom::ExecutionWorld::kIsolated,
            /*world_id=*/std::nullopt,
            blink::mojom::WantResultOption::kWantResult,
            blink::mojom::UserActivationOption::kDoNotActivate,
            blink::mojom::PromiseResultOption::kAwait)),
        ScriptExecutor::INCLUDE_SUB_FRAMES, {frame1_id, frame2_id},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
        GURL() /* webview_src */, helper.GetCallback());
    helper.Wait();

    EXPECT_THAT(helper.results(),
                testing::UnorderedElementsAre(
                    get_result_matcher(frame1_result, frame1_id, frame1_url),
                    get_result_matcher(frame2_result, frame2_id, frame2_url),
                    get_result_matcher(frame2_child_result, frame2_child_id,
                                       frame2_child_url)));
  }

  // Note: we don't use ExtensionApiFrameIdMap::kInvalidFrameId because we want
  // to target a "potentially valid" frame (emulating a frame that used to
  // exist, but no longer does).
  constexpr int kNonExistentFrameId = 99999;
  EXPECT_EQ(nullptr, ExtensionApiFrameIdMap::GetRenderFrameHostById(
                         web_contents, kNonExistentFrameId));

  {
    // Try injecting into multiple frames when one of the specified frames
    // doesn't exist.
    ScriptExecutorHelper helper;
    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(mojom::JSSource::New(kCode, GURL()));
    script_executor.ExecuteScript(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
        mojom::CodeInjection::NewJs(mojom::JSInjection::New(
            std::move(sources), mojom::ExecutionWorld::kIsolated,
            /*world_id=*/std::nullopt,
            blink::mojom::WantResultOption::kWantResult,
            blink::mojom::UserActivationOption::kDoNotActivate,
            blink::mojom::PromiseResultOption::kAwait)),
        ScriptExecutor::SPECIFIED_FRAMES,
        {frame1_id, frame2_id, kNonExistentFrameId},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
        GURL() /* webview_src */, helper.GetCallback());
    helper.Wait();

    EXPECT_THAT(helper.results(),
                testing::UnorderedElementsAre(
                    get_result_matcher(frame1_result, frame1_id, frame1_url),
                    get_result_matcher(frame2_result, frame2_id, frame2_url),
                    get_result_matcher(base::Value(), kNonExistentFrameId,
                                       GURL(), "No frame with ID: 99999")));
  }

  {
    // Try injecting into a single non-existent frame.
    ScriptExecutorHelper helper;
    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(mojom::JSSource::New(kCode, GURL()));
    script_executor.ExecuteScript(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id()),
        mojom::CodeInjection::NewJs(mojom::JSInjection::New(
            std::move(sources), mojom::ExecutionWorld::kIsolated,
            /*world_id=*/std::nullopt,
            blink::mojom::WantResultOption::kWantResult,
            blink::mojom::UserActivationOption::kDoNotActivate,
            blink::mojom::PromiseResultOption::kAwait)),
        ScriptExecutor::SPECIFIED_FRAMES, {kNonExistentFrameId},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
        GURL() /* webview_src */, helper.GetCallback());
    helper.Wait();

    EXPECT_THAT(helper.results(),
                testing::UnorderedElementsAre(
                    get_result_matcher(base::Value(), kNonExistentFrameId,
                                       GURL(), "No frame with ID: 99999")));
  }
}

}  // namespace extensions
