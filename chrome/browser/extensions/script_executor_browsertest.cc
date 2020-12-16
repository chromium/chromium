// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/extension_builder.h"
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

  const std::string& error() const { return error_; }
  const GURL& url() const { return url_; }
  const base::Value& result() const { return result_; }

 private:
  void OnScriptFinished(const std::string& error,
                        const GURL& url,
                        const base::ListValue& result) {
    error_ = error;
    url_ = url;
    result_ = result.Clone();
    run_loop_.Quit();
  }

  // Note: we include an initial value for the error so that asserting it's
  // empty is meaningful.
  std::string error_{"<initial error>"};
  GURL url_;
  base::Value result_;
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
        ExtensionBuilder("extension").AddPermission(host_permission).Build();
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
        web_contents, base::BindRepeating(&content::FrameMatchesName, name));
  }
};

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
    ui_test_utils::NavigateToURL(browser(), example_com);
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
  script_executor.ExecuteScript(
      HostID(HostID::EXTENSIONS, extension->id()), UserScript::ADD_JAVASCRIPT,
      kCode, ScriptExecutor::SPECIFIED_FRAMES,
      {ExtensionApiFrameIdMap::kTopFrameId},
      ScriptExecutor::DONT_MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
      ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
      GURL() /* script_url */, false /* user_gesture */,
      base::nullopt /* css_origin */, ScriptExecutor::JSON_SERIALIZED_RESULT,
      helper.GetCallback());
  helper.Wait();
  EXPECT_EQ("New Title", base::UTF16ToUTF8(web_contents->GetTitle()));
  EXPECT_EQ(web_contents->GetLastCommittedURL(), helper.url());
  EXPECT_EQ("", helper.error());

  base::Value expected(base::Value::Type::LIST);
  expected.Append("OK");
  EXPECT_EQ(expected, helper.result());
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
    ui_test_utils::NavigateToURL(browser(), example_com);
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
  content::RenderFrameHost* frame2 = GetFrameByName(web_contents, "frame2");
  ASSERT_TRUE(frame2);
  const int frame2_id = ExtensionApiFrameIdMap::GetFrameId(frame2);
  content::RenderFrameHost* frame3 = GetFrameByName(web_contents, "frame3");
  ASSERT_TRUE(frame3);
  content::RenderFrameHost* frame2_child =
      GetFrameByName(web_contents, "frame2_child");
  ASSERT_TRUE(frame2_child);

  ScriptExecutor script_executor(web_contents);
  // Note: Since other tests verify the code's effects, here we just rely on the
  // execution result as an indication that it ran.
  constexpr char kCode[] = "document.title;";

  {
    // Execute in frames 1 and 2. These are the only frames for which we should
    // get a result.
    ScriptExecutorHelper helper;
    script_executor.ExecuteScript(
        HostID(HostID::EXTENSIONS, extension->id()), UserScript::ADD_JAVASCRIPT,
        kCode, ScriptExecutor::SPECIFIED_FRAMES, {frame1_id, frame2_id},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
        ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
        GURL() /* script_url */, false /* user_gesture */,
        base::nullopt /* css_origin */, ScriptExecutor::JSON_SERIALIZED_RESULT,
        helper.GetCallback());
    helper.Wait();
    EXPECT_EQ("", helper.error());

    base::Value frame1_result("Frame 1");
    base::Value frame2_result("Frame 2");
    EXPECT_THAT(
        helper.result().GetList(),
        testing::UnorderedElementsAre(::testing::Eq(std::cref(frame1_result)),
                                      ::testing::Eq(std::cref(frame2_result))));
  }

  {
    // Repeat the execution in frames 1 and 2, but include subframes. This
    // should result in frame2_child being added to the results.
    ScriptExecutorHelper helper;
    script_executor.ExecuteScript(
        HostID(HostID::EXTENSIONS, extension->id()), UserScript::ADD_JAVASCRIPT,
        kCode, ScriptExecutor::INCLUDE_SUB_FRAMES, {frame1_id, frame2_id},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
        ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
        GURL() /* script_url */, false /* user_gesture */,
        base::nullopt /* css_origin */, ScriptExecutor::JSON_SERIALIZED_RESULT,
        helper.GetCallback());
    helper.Wait();
    EXPECT_EQ("", helper.error());

    base::Value frame1_result("Frame 1");
    base::Value frame2_result("Frame 2");
    base::Value frame2_child_result("Frame 2 Child");
    EXPECT_THAT(helper.result().GetList(),
                testing::UnorderedElementsAre(
                    ::testing::Eq(std::cref(frame1_result)),
                    ::testing::Eq(std::cref(frame2_result)),
                    ::testing::Eq(std::cref(frame2_child_result))));
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
    script_executor.ExecuteScript(
        HostID(HostID::EXTENSIONS, extension->id()), UserScript::ADD_JAVASCRIPT,
        kCode, ScriptExecutor::SPECIFIED_FRAMES,
        {frame1_id, frame2_id, kNonExistentFrameId},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
        ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
        GURL() /* script_url */, false /* user_gesture */,
        base::nullopt /* css_origin */, ScriptExecutor::JSON_SERIALIZED_RESULT,
        helper.GetCallback());
    helper.Wait();

    // When specifying multiple frames, if one doesn't exist, the rest of the
    // injections should succeed and there should be no error.
    EXPECT_EQ("", helper.error());
    base::Value frame1_result("Frame 1");
    base::Value frame2_result("Frame 2");
    EXPECT_THAT(
        helper.result().GetList(),
        testing::UnorderedElementsAre(::testing::Eq(std::cref(frame1_result)),
                                      ::testing::Eq(std::cref(frame2_result))));
  }

  {
    // Try injecting into a single non-existent frame.
    ScriptExecutorHelper helper;
    script_executor.ExecuteScript(
        HostID(HostID::EXTENSIONS, extension->id()), UserScript::ADD_JAVASCRIPT,
        kCode, ScriptExecutor::SPECIFIED_FRAMES, {kNonExistentFrameId},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
        ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
        GURL() /* script_url */, false /* user_gesture */,
        base::nullopt /* css_origin */, ScriptExecutor::JSON_SERIALIZED_RESULT,
        helper.GetCallback());
    helper.Wait();

    // If only a single frame was specified and it doesn't exist, the call
    // should return an error.
    EXPECT_EQ("The frame was removed.", helper.error());
    EXPECT_TRUE(helper.result().GetList().empty());
  }
}

}  // namespace extensions
