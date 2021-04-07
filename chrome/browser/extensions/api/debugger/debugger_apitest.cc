// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class DebuggerApiTest : public ExtensionApiTest {
 protected:
  ~DebuggerApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // Run the attach function. If |expected_error| is not empty, then the
  // function should fail with the error. Otherwise, the function is expected
  // to succeed.
  testing::AssertionResult RunAttachFunction(const GURL& url,
                                             const std::string& expected_error);

  const Extension* extension() const { return extension_.get(); }
  base::CommandLine* command_line() const { return command_line_; }

  void AdvanceClock(base::TimeDelta time) { clock_.Advance(time); }

 private:
  testing::AssertionResult RunAttachFunctionOnTarget(
      const std::string& debuggee_target, const std::string& expected_error);

  // The command-line for the test process, preserved in order to modify
  // mid-test.
  base::CommandLine* command_line_;

  // A basic extension with the debugger permission.
  scoped_refptr<const Extension> extension_;

  // A temporary directory in which to create and load from the
  // |extension_|.
  TestExtensionDir test_extension_dir_;
  base::SimpleTestTickClock clock_;
};

void DebuggerApiTest::SetUpCommandLine(base::CommandLine* command_line) {
  ExtensionApiTest::SetUpCommandLine(command_line);
  // We need to hold onto |command_line| in order to modify it during the test.
  command_line_ = command_line;
}

void DebuggerApiTest::SetUpOnMainThread() {
  ExtensionApiTest::SetUpOnMainThread();
  test_extension_dir_.WriteManifest(
      R"({
         "name": "debugger",
         "version": "0.1",
         "manifest_version": 2,
         "permissions": ["debugger"]
       })");
  test_extension_dir_.WriteFile(FILE_PATH_LITERAL("test_file.html"),
                                "<html>Hello world!</html>");
  extension_ = LoadExtension(test_extension_dir_.UnpackedPath());
  ASSERT_TRUE(extension_);
}

testing::AssertionResult DebuggerApiTest::RunAttachFunction(
    const GURL& url, const std::string& expected_error) {
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Attach by tabId.
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  std::string debugee_by_tab = base::StringPrintf("{\"tabId\": %d}", tab_id);
  testing::AssertionResult result =
      RunAttachFunctionOnTarget(debugee_by_tab, expected_error);
  if (!result)
    return result;

  // Attach by targetId.
  scoped_refptr<DebuggerGetTargetsFunction> get_targets_function =
      new DebuggerGetTargetsFunction();
  std::unique_ptr<base::Value> value(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_targets_function.get(), "[]", browser()));
  base::ListValue* targets = nullptr;
  EXPECT_TRUE(value->GetAsList(&targets));

  std::string debugger_target_id;
  for (size_t i = 0; i < targets->GetSize(); ++i) {
    base::DictionaryValue* target_dict = nullptr;
    EXPECT_TRUE(targets->GetDictionary(i, &target_dict));
    int id = -1;
    if (target_dict->GetInteger("tabId", &id) && id == tab_id) {
      EXPECT_TRUE(target_dict->GetString("id", &debugger_target_id));
      break;
    }
  }
  EXPECT_TRUE(!debugger_target_id.empty());

  std::string debugee_by_target_id =
      base::StringPrintf("{\"targetId\": \"%s\"}", debugger_target_id.c_str());
  return RunAttachFunctionOnTarget(debugee_by_target_id, expected_error);
}

testing::AssertionResult DebuggerApiTest::RunAttachFunctionOnTarget(
    const std::string& debuggee_target, const std::string& expected_error) {
  scoped_refptr<DebuggerAttachFunction> attach_function =
      new DebuggerAttachFunction();
  attach_function->set_extension(extension_.get());

  std::string actual_error;
  if (!extension_function_test_utils::RunFunction(
          attach_function.get(),
          base::StringPrintf("[%s, \"1.1\"]", debuggee_target.c_str()),
          browser(), api_test_utils::NONE)) {
    actual_error = attach_function->GetError();
  } else {
    // Clean up and detach.
    scoped_refptr<DebuggerDetachFunction> detach_function =
        new DebuggerDetachFunction();
    detach_function->set_extension(extension_.get());
    if (!extension_function_test_utils::RunFunction(
            detach_function.get(),
            base::StringPrintf("[%s]", debuggee_target.c_str()), browser(),
            api_test_utils::NONE)) {
      return testing::AssertionFailure() << "Could not detach from "
          << debuggee_target << " : " << detach_function->GetError();
    }
  }

  if (expected_error.empty() && !actual_error.empty()) {
    return testing::AssertionFailure() << "Could not attach to "
        << debuggee_target << " : " << actual_error;
  } else if (actual_error != expected_error) {
    return testing::AssertionFailure() << "Did not get correct error upon "
        << "attach to " << debuggee_target << " : "
        << "expected: " << expected_error << ", found: " << actual_error;
  }
  return testing::AssertionSuccess();
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerNotAllowedOnOtherExtensionPages) {
  // Load another arbitrary extension with an associated resource (popup.html).
  base::FilePath path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").AppendASCII("simple_with_popup");
  const Extension* another_extension = LoadExtension(path);
  ASSERT_TRUE(another_extension);

  GURL other_ext_url = another_extension->GetResourceURL("popup.html");

  // This extension should not be able to access another extension.
  EXPECT_TRUE(RunAttachFunction(
      other_ext_url, manifest_errors::kCannotAccessExtensionUrl));

  // This extension *should* be able to debug itself.
  EXPECT_TRUE(RunAttachFunction(extension()->GetResourceURL("test_file.html"),
                                std::string()));

  // Append extensions on chrome urls switch. The extension should now be able
  // to debug any extension.
  command_line()->AppendSwitch(switches::kExtensionsOnChromeURLs);
  EXPECT_TRUE(RunAttachFunction(other_ext_url, std::string()));
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerAllowedOnFileUrlsWithFileAccess) {
  EXPECT_TRUE(RunExtensionTest(
      {.name = "debugger_file_access", .custom_arg = "enabled"},
      {.allow_file_access = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerNotAllowedOnFileUrlsWithoutAccess) {
  EXPECT_TRUE(RunExtensionTest("debugger_file_access")) << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest, InfoBar) {
  int tab_id = sessions::SessionTabHelper::IdForTab(
                   browser()->tab_strip_model()->GetActiveWebContents())
                   .id();
  scoped_refptr<DebuggerAttachFunction> attach_function;
  scoped_refptr<DebuggerDetachFunction> detach_function;

  Browser* another_browser =
      Browser::Create(Browser::CreateParams(profile(), true));
  AddBlankTabAndShow(another_browser);
  AddBlankTabAndShow(another_browser);
  int tab_id2 = sessions::SessionTabHelper::IdForTab(
                    another_browser->tab_strip_model()->GetActiveWebContents())
                    .id();

  InfoBarService* service1 = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  InfoBarService* service2 = InfoBarService::FromWebContents(
      another_browser->tab_strip_model()->GetWebContentsAt(0));
  InfoBarService* service3 = InfoBarService::FromWebContents(
      another_browser->tab_strip_model()->GetWebContentsAt(1));

  // Attaching to one tab should create infobars in both browsers.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), browser(),
      api_test_utils::NONE));
  EXPECT_EQ(1u, service1->infobar_count());
  EXPECT_EQ(1u, service2->infobar_count());
  EXPECT_EQ(1u, service3->infobar_count());

  // Attaching to another tab should not create more infobars.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id2), browser(),
      api_test_utils::NONE));
  EXPECT_EQ(1u, service1->infobar_count());
  EXPECT_EQ(1u, service2->infobar_count());
  EXPECT_EQ(1u, service3->infobar_count());

  // Detaching from one of the tabs should not remove infobars.
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id2),
      browser(), api_test_utils::NONE));
  EXPECT_EQ(1u, service1->infobar_count());
  EXPECT_EQ(1u, service2->infobar_count());
  EXPECT_EQ(1u, service3->infobar_count());

  // Detaching from the other tab also should not remove infobars, since even
  // though there is no longer an extension attached, the infobar can only be
  // dismissed by explicit user action.
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      browser(), api_test_utils::NONE));
  EXPECT_EQ(1u, service1->infobar_count());
  EXPECT_EQ(1u, service2->infobar_count());
  EXPECT_EQ(1u, service3->infobar_count());

  // Attach again; should not create infobars.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), browser(),
      api_test_utils::NONE));
  EXPECT_EQ(1u, service1->infobar_count());
  EXPECT_EQ(1u, service2->infobar_count());
  EXPECT_EQ(1u, service3->infobar_count());

  // Remove the global infobar by simulating what happens when the user clicks
  // the close button (see InfoBarView::ButtonPressed()).  The
  // InfoBarDismissed() call will remove the infobars everywhere except on
  // |service2| itself; the RemoveSelf() call removes that one.
  service2->infobar_at(0)->delegate()->InfoBarDismissed();
  service2->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0u, service1->infobar_count());
  EXPECT_EQ(0u, service2->infobar_count());
  EXPECT_EQ(0u, service3->infobar_count());
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  // Cannot detach again.
  ASSERT_FALSE(extension_function_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      browser(), api_test_utils::NONE));

  // Attaching once again should create a new infobar.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), browser(),
      api_test_utils::NONE));
  EXPECT_EQ(1u, service1->infobar_count());
  EXPECT_EQ(1u, service2->infobar_count());
  EXPECT_EQ(1u, service3->infobar_count());

  // Closing tab should not affect anything.
  ASSERT_TRUE(another_browser->tab_strip_model()->CloseWebContentsAt(1, 0));
  service3 = nullptr;
  EXPECT_EQ(1u, service1->infobar_count());
  EXPECT_EQ(1u, service2->infobar_count());

  // Closing browser should not affect anything.
  CloseBrowserSynchronously(another_browser);
  service2 = nullptr;
  another_browser = nullptr;
  EXPECT_EQ(1u, service1->infobar_count());

  // Detach should not affect anything.
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      browser(), api_test_utils::NONE));
  EXPECT_EQ(1u, service1->infobar_count());
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest, InfoBarIsRemovedAfterFiveSeconds) {
  int tab_id = sessions::SessionTabHelper::IdForTab(
                   browser()->tab_strip_model()->GetActiveWebContents())
                   .id();
  InfoBarService* service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Attaching to the tab should create an infobar.
  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), browser(),
      api_test_utils::NONE));
  EXPECT_EQ(1u, service->infobar_count());

  // Detaching from the tab should remove the infobar after 5 seconds.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      browser(), api_test_utils::NONE));

  // Even though the extension detached, the infobar should not detach
  // immediately, and should remain visible for 5 seconds to ensure the user
  // has an opportunity to see it.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
  EXPECT_EQ(1u, service->infobar_count());  // Infobar is still shown.

  // Advance the clock by 5 seconds, and verify the infobar is removed.
  AdvanceClock(ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
  run_loop.Run();

  EXPECT_EQ(0u, service->infobar_count());
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       InfoBarIsNotRemovedIfAttachAgainBeforeFiveSeconds) {
  int tab_id = sessions::SessionTabHelper::IdForTab(
                   browser()->tab_strip_model()->GetActiveWebContents())
                   .id();
  InfoBarService* service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Attaching to the tab should create an infobar.
  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), browser(),
      api_test_utils::NONE));
  EXPECT_EQ(1u, service->infobar_count());

  // Detaching from the tab and attaching it again before 5 seconds should not
  // remove the infobar.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      browser(), api_test_utils::NONE));
  EXPECT_EQ(1u, service->infobar_count());

  attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), browser(),
      api_test_utils::NONE));
  // Verify that only one infobar is created.
  EXPECT_EQ(1u, service->infobar_count());

  // Verify that infobar is not closed after 5 seconds.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
  AdvanceClock(ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
  run_loop.Run();

  EXPECT_EQ(1u, service->infobar_count());
}

// Tests that policy blocked hosts supersede the `debugger`
// permission. Regression test for crbug.com/1139156.
IN_PROC_BROWSER_TEST_F(DebuggerApiTest, TestDefaultPolicyBlockedHosts) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url("https://example.com");
  EXPECT_TRUE(RunAttachFunction(url, std::string()));
  policy::MockConfigurationPolicyProvider policy_provider;
  ExtensionManagementPolicyUpdater pref(&policy_provider);
  pref.AddPolicyBlockedHost("*", url.spec());
  EXPECT_FALSE(
      RunAttachFunction(url, manifest_errors::kCannotAccessExtensionUrl));
}

class DebuggerExtensionApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, Debugger) {
  ASSERT_TRUE(RunExtensionTest("debugger")) << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, ParentTargetPermissions) {
  // Run test with file access disabled.
  ASSERT_TRUE(RunExtensionTest("parent_target_permissions")) << message_;
}

// Tests that an extension is not allowed to inspect a worker through the
// inspectWorker debugger command.
// Regression test for https://crbug.com/1059577.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest,
                       DebuggerNotAllowedToInvokeInspectWorker) {
  GURL url(embedded_test_server()->GetURL(
      "/extensions/api_test/debugger_inspect_worker/inspected_page.html"));

  EXPECT_TRUE(RunExtensionTest(
      {.name = "debugger_inspect_worker", .custom_arg = url.spec().c_str()}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, AttachToEmptyUrls) {
  ASSERT_TRUE(RunExtensionTest("debugger_attach_to_empty_urls")) << message_;
}

// Tests that navigation to a forbidden URL is properly denied and
// does not cause a crash.
// This is a regression test for https://crbug.com/1188889.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, NavigateToForbiddenUrl) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  ASSERT_TRUE(RunExtensionTest("debugger_navigate_to_forbidden_url"))
      << message_;
}

class SitePerProcessDebuggerExtensionApiTest : public DebuggerExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DebuggerExtensionApiTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest, Debugger) {
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/extensions/api_test/debugger/oopif.html"));
  GURL iframe_url(embedded_test_server()->GetURL(
      "b.com", "/extensions/api_test/debugger/oopif_frame.html"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationManager navigation_manager(tab, url);
  content::TestNavigationManager navigation_manager_iframe(tab, iframe_url);
  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());
  navigation_manager.WaitForNavigationFinished();
  navigation_manager_iframe.WaitForNavigationFinished();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ASSERT_TRUE(RunExtensionTest(
      {.name = "debugger", .custom_arg = "oopif.html;oopif_frame.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       NavigateSubframe) {
  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/extensions/api_test/debugger_navigate_subframe/inspected_page.html"));
  ASSERT_TRUE(RunExtensionTest(
      {.name = "debugger_navigate_subframe", .custom_arg = url.spec().c_str()}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       AutoAttachPermissions) {
  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/extensions/api_test/debugger_auto_attach_permissions/page.html"));
  ASSERT_TRUE(RunExtensionTest({.name = "debugger_auto_attach_permissions",
                                .custom_arg = url.spec().c_str()}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       DebuggerCheckInnerUrl) {
  ASSERT_TRUE(RunExtensionTest("debugger_check_inner_url")) << message_;
}

}  // namespace extensions
