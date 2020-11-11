// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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
#include "url/gurl.h"

namespace extensions {

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

  std::string execution_error("<initial error>");
  GURL execution_url;
  base::Value execution_result;
  base::RunLoop run_loop;
  auto script_finished = [&execution_error, &execution_url, &execution_result,
                          &run_loop](const std::string& error, const GURL& url,
                                     const base::ListValue& value) {
    execution_error = error;
    execution_url = url;
    execution_result = value.Clone();
    run_loop.Quit();
  };

  script_executor.ExecuteScript(
      HostID(HostID::EXTENSIONS, extension->id()), UserScript::ADD_JAVASCRIPT,
      kCode, ScriptExecutor::SINGLE_FRAME, ExtensionApiFrameIdMap::kTopFrameId,
      ScriptExecutor::DONT_MATCH_ABOUT_BLANK, UserScript::DOCUMENT_IDLE,
      ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
      GURL() /* script_url */, false /* user_gesture */,
      base::nullopt /* css_origin */, ScriptExecutor::JSON_SERIALIZED_RESULT,
      base::BindLambdaForTesting(script_finished));
  run_loop.Run();
  EXPECT_EQ("New Title", base::UTF16ToUTF8(web_contents->GetTitle()));
  EXPECT_EQ(web_contents->GetLastCommittedURL(), execution_url);
  EXPECT_EQ("", execution_error);

  base::Value expected(base::Value::Type::LIST);
  expected.Append("OK");
  EXPECT_EQ(expected, execution_result);
}

}  // namespace extensions
