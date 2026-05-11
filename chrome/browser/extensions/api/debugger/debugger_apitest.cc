// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/browser_closed_waiter.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "pdf/buildflags.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/base/base_window.h"

#if BUILDFLAG(ENABLE_PDF)
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/test_mime_handler_stream_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Gets all URLs from the list of targets, with the ports removed.
std::vector<std::string> GetTargetUrlsWithoutPorts(
    const base::ListValue& targets) {
  return base::ToVector(targets, [](const base::Value& value) {
    GURL::Replacements remove_port;
    remove_port.ClearPort();
    const std::string* url = value.GetDict().FindString("url");
    return url ? GURL(*url).ReplaceComponents(remove_port).spec()
               : "<missing field>";
  });
}

// Gets all targets as a list from the context.
// This method also filters out targets with "chrome://" URLs, such as the
// internal WebUI toolbar, to ensure tests are robust against the presence of
// these internal targets.
base::ListValue RunGetTargets(
    content::BrowserContext* context,
    api_test_utils::FunctionMode mode = api_test_utils::FunctionMode::kNone) {
  auto get_targets_function =
      base::MakeRefCounted<DebuggerGetTargetsFunction>();
  std::optional<base::Value> value =
      api_test_utils::RunFunctionAndReturnSingleResult(
          get_targets_function.get(), "[]", context, mode);
  EXPECT_THAT(
      value, testing::Optional(testing::Property(&base::Value::is_list, true)));
  base::ListValue targets = std::move(*value).TakeList();
  targets.EraseIf([](const base::Value& target) {
    const std::string* url = target.GetDict().FindString("url");
    return url && GURL(*url).SchemeIs(content::kChromeUIScheme);
  });
  return targets;
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

using testing::ElementsAre;
using testing::Eq;

class DebuggerApiTest : public ExtensionApiTest {
 protected:
  ~DebuggerApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // Run the attach function. If |expected_error| is not empty, then the
  // function should fail with the error. Otherwise, the function is expected
  // to succeed. If `ignore_navigation_errors` then navigation errors will be
  // ignored.
  testing::AssertionResult RunAttachFunction(
      const GURL& url,
      const std::string& expected_error,
      bool ignore_navigation_errors = false);
  testing::AssertionResult RunAttachFunction(
      const content::WebContents* web_contents,
      const std::string& expected_error);

  const Extension* extension() const { return extension_.get(); }
  base::CommandLine* command_line() const { return command_line_; }

  void AdvanceClock(base::TimeDelta time) { clock_.Advance(time); }

 private:
  testing::AssertionResult RunAttachFunctionOnTarget(
      const std::string& debuggee_target, const std::string& expected_error);

  // The command-line for the test process, preserved in order to modify
  // mid-test.
  raw_ptr<base::CommandLine> command_line_;

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

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

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
    const GURL& url,
    const std::string& expected_error,
    bool ignore_navigation_errors) {
  auto* web_contents = GetActiveWebContents();
  bool navigation_result = NavigateToURL(web_contents, url);
  // Most navigations should succeed, but some are allowed to fail.
  if (!ignore_navigation_errors) {
    EXPECT_TRUE(navigation_result);
  }
  return RunAttachFunction(web_contents, expected_error);
}

testing::AssertionResult DebuggerApiTest::RunAttachFunction(
    const content::WebContents* web_contents,
    const std::string& expected_error) {
  // Attach by tabId.
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  std::string debugee_by_tab = base::StringPrintf("{\"tabId\": %d}", tab_id);
  testing::AssertionResult result =
      RunAttachFunctionOnTarget(debugee_by_tab, expected_error);
  if (!result) {
    return result;
  }

  // Attach by targetId.
  scoped_refptr<DebuggerGetTargetsFunction> get_targets_function =
      new DebuggerGetTargetsFunction();
  std::optional<base::Value> value(
      api_test_utils::RunFunctionAndReturnSingleResult(
          get_targets_function.get(), "[]", profile()));
  EXPECT_TRUE(value->is_list());

  std::string debugger_target_id;
  for (const base::Value& target_value : value->GetList()) {
    EXPECT_TRUE(target_value.is_dict());
    std::optional<int> id = target_value.GetDict().FindInt("tabId");
    if (id == tab_id) {
      const std::string* id_str = target_value.GetDict().FindString("id");
      EXPECT_TRUE(id_str);
      debugger_target_id = *id_str;
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
  if (!api_test_utils::RunFunction(
          attach_function.get(),
          base::StringPrintf("[%s, \"1.1\"]", debuggee_target.c_str()),
          profile())) {
    actual_error = attach_function->GetError();
  } else {
    // Clean up and detach.
    scoped_refptr<DebuggerDetachFunction> detach_function =
        new DebuggerDetachFunction();
    detach_function->set_extension(extension_.get());
    if (!api_test_utils::RunFunction(
            detach_function.get(),
            base::StringPrintf("[%s]", debuggee_target.c_str()), profile())) {
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
  EXPECT_TRUE(RunExtensionTest("debugger_file_access",
                               {.custom_arg = "enabled"},
                               {.allow_file_access = true}))
      << message_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/441339825): Fails on desktop Android with an error about
// access to localhost.
IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerNotAllowedOnFileUrlsWithoutAccess) {
  EXPECT_TRUE(RunExtensionTest("debugger_file_access")) << message_;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       BrowserTargetAllowedForUnpackedPerfettoUIWithFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kAllowUnpackedPerfettoExtension);

  scoped_refptr<const Extension> unpacked_perfetto =
      ExtensionBuilder("Perfetto UI")
          .SetID(extension_misc::kPerfettoUIExtensionId)
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .AddAPIPermission("debugger")
          .Build();

  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(unpacked_perfetto.get());

  EXPECT_TRUE(api_test_utils::RunFunction(
      attach_function.get(), R"([{"targetId": "browser"}, "1.1"])", profile()))
      << attach_function->GetError();

  // Clean up and detach.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(unpacked_perfetto.get());
  EXPECT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), R"([{"targetId": "browser"}])", profile()));
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       BrowserTargetNotAllowedForUnpackedPerfettoUI) {
  scoped_refptr<const Extension> unpacked_perfetto =
      ExtensionBuilder("Perfetto UI")
          .SetID(extension_misc::kPerfettoUIExtensionId)
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .AddAPIPermission("debugger")
          .Build();

  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(unpacked_perfetto.get());

  std::string actual_error = api_test_utils::RunFunctionAndReturnError(
      attach_function.get(), R"([{"targetId": "browser"}, "1.1"])", profile());

  EXPECT_EQ("No target with given id browser.", actual_error);
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       BrowserTargetAllowedForComponentPerfettoUI) {
  scoped_refptr<const Extension> component_perfetto =
      ExtensionBuilder("Perfetto UI")
          .SetID(extension_misc::kPerfettoUIExtensionId)
          .SetLocation(mojom::ManifestLocation::kComponent)
          .AddAPIPermission("debugger")
          .Build();

  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(component_perfetto.get());

  EXPECT_TRUE(api_test_utils::RunFunction(
      attach_function.get(), R"([{"targetId": "browser"}, "1.1"])", profile()))
      << attach_function->GetError();

  // Now, try to attach to a WebUI page via Target.attachToTarget through the
  // browser target (which acts as a root session). This should be blocked by
  // the child session's delegated MayAttachToRenderFrameHost check.

  // 1. Open a WebUI tab.
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://version")));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // 2. Find the targetId for the WebUI tab.
  scoped_refptr<DebuggerGetTargetsFunction> get_targets =
      new DebuggerGetTargetsFunction();
  std::optional<base::Value> targets_value(
      api_test_utils::RunFunctionAndReturnSingleResult(get_targets.get(), "[]",
                                                       profile()));
  ASSERT_TRUE(targets_value->is_list());

  std::string webui_target_id;
  for (const base::Value& target_value : targets_value->GetList()) {
    std::optional<int> id = target_value.GetDict().FindInt("tabId");
    if (id == tab_id) {
      const std::string* id_str = target_value.GetDict().FindString("id");
      ASSERT_TRUE(id_str);
      webui_target_id = *id_str;
      break;
    }
  }
  ASSERT_FALSE(webui_target_id.empty());

  // 3. Send Target.attachToTarget.
  auto send_command = base::MakeRefCounted<DebuggerSendCommandFunction>();
  send_command->set_extension(component_perfetto.get());

  std::string command_args = base::StringPrintf(
      R"([{"targetId": "browser"}, "Target.attachToTarget", )"
      R"({"targetId": "%s"}])",
      webui_target_id.c_str());

  // Run the command and expect it to fail (it will return an error response).
  std::string attach_error = api_test_utils::RunFunctionAndReturnError(
      send_command.get(), command_args, profile());

  // The attach should fail with an error because the delegated
  // MayAttachToRenderFrameHost check will block attaching to the WebUI frame.
  EXPECT_THAT(attach_error, testing::HasSubstr("Not allowed"));

  // Clean up and detach.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(component_perfetto.get());
  EXPECT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), R"([{"targetId": "browser"}])", profile()));
}

class TestInterstitialPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  TestInterstitialPage(content::WebContents* web_contents,
                       const GURL& request_url)
      : SecurityInterstitialPage(
            web_contents,
            request_url,
            std::make_unique<
                security_interstitials::SecurityInterstitialControllerClient>(
                web_contents,
                CreateTestMetricsHelper(web_contents),
                nullptr,
                base::i18n::GetConfiguredLocale(),
                GURL(),
                /* settings_page_helper*/ nullptr)) {}

  ~TestInterstitialPage() override = default;
  void OnInterstitialClosing() override {}

 protected:
  void PopulateInterstitialStrings(base::DictValue& load_time_data) override {}

  std::unique_ptr<security_interstitials::MetricsHelper>
  CreateTestMetricsHelper(content::WebContents* web_contents) {
    security_interstitials::MetricsHelper::ReportDetails report_details;
    report_details.metric_prefix = "test_blocking_page";
    return std::make_unique<security_interstitials::MetricsHelper>(
        GURL(), report_details, nullptr);
  }
};

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerNotAllowedOnRestrictedBlobUrls) {
  content::WebContents* web_contents = GetActiveWebContents();
  // Use chrome://version because it is webui on Win/Mac/Linux and Android.
  EXPECT_TRUE(content::NavigateToURL(web_contents, GURL("chrome://version")));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    var blob = new Blob([JSON.stringify({foo: 'bar'})], {
      type: "application/json",
    });
    var burl = URL.createObjectURL(blob, 'application/json');
    window.open(burl);
  )"));
  content::WebContents* blob_web_contents = GetActiveWebContents();
  EXPECT_NE(blob_web_contents, web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(blob_web_contents));
  EXPECT_EQ("{\"foo\":\"bar\"}",
            content::EvalJs(blob_web_contents, "document.body.innerText"));
  EXPECT_TRUE(
      RunAttachFunction(blob_web_contents, "Cannot access a chrome:// URL"));
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerNotAllowedOnPolicyRestrictedBlobUrls) {
  GURL url(embedded_test_server()->GetURL("a.com", "/simple.html"));
  content::WebContents* web_contents = GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  ASSERT_TRUE(content::ExecJs(web_contents, R"(
    var blob = new Blob([JSON.stringify({foo: 'bar'})], {
      type: "application/json",
    });
    window.open(URL.createObjectURL(blob, 'application/json'));
  )"));
  content::WebContents* blob_web_contents = GetActiveWebContents();
  EXPECT_NE(blob_web_contents, web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(blob_web_contents));
  EXPECT_EQ("{\"foo\":\"bar\"}",
            content::EvalJs(blob_web_contents, "document.body.innerText"));
  base::RunLoop run_loop;
  URLPatternSet default_blocked_hosts;
  default_blocked_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://a.com/*"));
  PermissionsData::SetDefaultPolicyHostRestrictions(
      util::GetBrowserContextId(profile()), default_blocked_hosts,
      URLPatternSet());
  EXPECT_TRUE(
      RunAttachFunction(blob_web_contents, "Cannot attach to this target."));
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerNotAllowedOnSecurityInterstitials) {
  content::WebContents* web_contents = GetActiveWebContents();
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      std::make_unique<content::MockNavigationHandle>(
          GURL("https://google.com/"), web_contents->GetPrimaryMainFrame());
  navigation_handle->set_has_committed(true);
  navigation_handle->set_is_same_document(false);
  EXPECT_TRUE(RunAttachFunction(web_contents, ""));

  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      navigation_handle.get(),
      std::make_unique<TestInterstitialPage>(web_contents, GURL()));
  security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
      web_contents)
      ->DidFinishNavigation(navigation_handle.get());

  EXPECT_TRUE(RunAttachFunction(web_contents, "Cannot attach to this target."));
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest, InfoBar) {
  int tab_id =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();
  scoped_refptr<DebuggerAttachFunction> attach_function;
  scoped_refptr<DebuggerDetachFunction> detach_function;

  // Create a second browser with two tabs.
  BrowserWindowInterface* browser2 =
      CreateBrowserWindowWithType(BrowserWindowInterface::TYPE_NORMAL);
  TabListInterface* tab_list2 = TabListInterface::From(browser2);
  // Platforms like Win/Mac/Linux create browsers with no tabs, whereas Android
  // creates browsers with a single tab.
  if (tab_list2->GetTabCount() == 0) {
    tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  }
  tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  ASSERT_EQ(2, tab_list2->GetTabCount());
  int tab_id2 = sessions::SessionTabHelper::IdForTab(
                    tab_list2->GetActiveTab()->GetContents())
                    .id();

  infobars::ContentInfoBarManager* manager1 =
      infobars::ContentInfoBarManager::FromWebContents(GetActiveWebContents());
  infobars::ContentInfoBarManager* manager2 =
      infobars::ContentInfoBarManager::FromWebContents(
          tab_list2->GetTab(0)->GetContents());
  infobars::ContentInfoBarManager* manager3 =
      infobars::ContentInfoBarManager::FromWebContents(
          tab_list2->GetTab(1)->GetContents());

  // Attaching to one tab should create infobars in both browsers.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(1u, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Attaching to another tab should not create more infobars.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id2), profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(1u, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Detaching from one of the tabs should not remove infobars.
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id2),
      profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(1u, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Detaching from the other tab also should not remove infobars, since even
  // though there is no longer an extension attached, the infobar can only be
  // dismissed by explicit user action.
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(1u, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Attach again; should not create infobars.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(1u, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Remove the global infobar by simulating what happens when the user clicks
  // the close button (see InfoBarView::ButtonPressed()).  The
  // InfoBarDismissed() call will remove the infobars everywhere except on
  // |manager2| itself; the RemoveSelf() call removes that one.
  manager2->infobars()[0]->delegate()->InfoBarDismissed();
  manager2->infobars()[0]->RemoveSelf();
  EXPECT_EQ(0u, manager1->infobars().size());
  EXPECT_EQ(0u, manager2->infobars().size());
  EXPECT_EQ(0u, manager3->infobars().size());
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  // Cannot detach again.
  ASSERT_FALSE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      profile()));

  // Attaching once again should create a new infobar.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(1u, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Closing tab should not affect anything.
  EXPECT_EQ(2, tab_list2->GetTabCount());
  tab_list2->CloseTab(tab_list2->GetTab(1)->GetHandle());
  EXPECT_EQ(1, tab_list2->GetTabCount());
  manager3 = nullptr;
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(1u, manager2->infobars().size());

  // Closing browser should not affect anything. Use a waiter because browser
  // close is async on some platforms (e.g. Android).
  BrowserClosedWaiter waiter(browser2);
  browser2->GetWindow()->Close();
  waiter.Wait();
  manager2 = nullptr;
  browser2 = nullptr;
  EXPECT_EQ(1u, manager1->infobars().size());

  // Detach should not affect anything.
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest, InfoBarIsRemovedAfterFiveSeconds) {
  int tab_id =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(GetActiveWebContents());

  // Attaching to the tab should create an infobar.
  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  EXPECT_EQ(1u, manager->infobars().size());

  // Detaching from the tab should remove the infobar after 5 seconds.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      profile()));

  // Even though the extension detached, the infobar should not detach
  // immediately, and should remain visible for 5 seconds to ensure the user
  // has an opportunity to see it.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
  EXPECT_EQ(1u, manager->infobars().size());  // Infobar is still shown.

  // Advance the clock by 5 seconds, and verify the infobar is removed.
  AdvanceClock(ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
  run_loop.Run();

  EXPECT_EQ(0u, manager->infobars().size());
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       InfoBarIsNotRemovedWhenAnotherDebuggerAttached) {
  const int tab_id1 =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(GetActiveWebContents());

  ASSERT_TRUE(embedded_test_server()->Started());
  ASSERT_TRUE(
      NavigateToURLInNewTab(embedded_test_server()->GetURL("/simple.html")));
  const int tab_id2 =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();

  // Attaching to a tab should create an infobar.
  {
    auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
    attach_function->set_extension(extension());
    ASSERT_TRUE(api_test_utils::RunFunction(
        attach_function.get(),
        base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id1), profile()));
  }

  EXPECT_EQ(1u, manager->infobars().size());

  // Attaching to a 2nd tab, to have another attached debugger.
  {
    auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
    attach_function->set_extension(extension());
    ASSERT_TRUE(api_test_utils::RunFunction(
        attach_function.get(),
        base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id2), profile()));
  }

  EXPECT_EQ(1u, manager->infobars().size());

  // Detaching from the tab should not remove the infobar after 5 seconds, as
  // another debugger is still attached.
  {
    auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
    detach_function->set_extension(extension());
    ASSERT_TRUE(api_test_utils::RunFunction(
        detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id1),
        profile()));
  }

  // Advance the clock by 5 seconds.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
    AdvanceClock(ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
    run_loop.Run();
  }

  // Verify inforbar not removed.
  EXPECT_EQ(1u, manager->infobars().size());

  // Now detach the last debugger.
  {
    auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
    detach_function->set_extension(extension());
    ASSERT_TRUE(api_test_utils::RunFunction(
        detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id2),
        profile()));
  }

  // Advance the clock by 5 seconds, once again.
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
    AdvanceClock(ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
    run_loop.Run();
  }

  // Verify inforbar removed.
  EXPECT_EQ(0u, manager->infobars().size());
}

#if !BUILDFLAG(IS_ANDROID)
// Android does not support multiple profiles in Chrome. User switching is
// handled at the OS level.
class CrossProfileDebuggerApiTest : public DebuggerApiTest {
 protected:
  Profile* other_profile() { return other_profile_; }
  Profile* otr_profile() { return otr_profile_; }

  std::unique_ptr<content::WebContents> CreateTabWithProfileAndNavigate(
      Profile* profile,
      const GURL& url) {
    auto wc = content::WebContents::Create(
        content::WebContents::CreateParams(profile));
    EXPECT_TRUE(content::NavigateToURL(wc.get(), url));
    return wc;
  }

 private:
  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS)
    ash::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
#endif  // BUILDFLAG(IS_CHROMEOS)
    DebuggerApiTest::SetUpOnMainThread();
    ProfileManager* const profile_manager = profile_util::GetProfileManager();

    other_profile_ = &profiles::testing::CreateProfileSync(
        profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
    otr_profile_ = profile()->GetPrimaryOTRProfile(true);
  }

  void TearDownOnMainThread() override {
    ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile_);
    DebuggerApiTest::TearDownOnMainThread();
  }

  raw_ptr<Profile, DanglingUntriaged> other_profile_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> otr_profile_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(CrossProfileDebuggerApiTest, GetTargets) {
  auto wc1 = CreateTabWithProfileAndNavigate(
      other_profile(),
      embedded_test_server()->GetURL("/simple.html?other_profile"));
  auto wc2 = CreateTabWithProfileAndNavigate(
      otr_profile(),
      embedded_test_server()->GetURL("/simple.html?off_the_record"));

  {
    base::ListValue targets = RunGetTargets(profile());
    EXPECT_THAT(targets, ElementsAre(base::test::DictionaryHasValue(
                             "url", base::Value("about:blank"))));
  }

  {
    const base::ListValue targets =
        RunGetTargets(profile(), api_test_utils::FunctionMode::kIncognito);
    std::vector<std::string> urls = GetTargetUrlsWithoutPorts(targets);
    EXPECT_THAT(urls, testing::UnorderedElementsAre(
                          "about:blank",
                          "http://127.0.0.1/simple.html?off_the_record"));
  }
}

IN_PROC_BROWSER_TEST_F(CrossProfileDebuggerApiTest, Attach) {
  auto wc1 = CreateTabWithProfileAndNavigate(
      other_profile(),
      embedded_test_server()->GetURL("/simple.html?other_profile"));
  std::string target_in_other_profile = base::StringPrintf(
      "[{\"targetId\": \"%s\"}, \"1.1\"]",
      content::DevToolsAgentHost::GetOrCreateFor(wc1.get())->GetId().c_str());

  {
    auto debugger_attach_function =
        base::MakeRefCounted<DebuggerAttachFunction>();
    debugger_attach_function->set_extension(extension());
    EXPECT_FALSE(api_test_utils::RunFunction(
        debugger_attach_function.get(), target_in_other_profile, profile()));
  }
  {
    auto debugger_attach_function =
        base::MakeRefCounted<DebuggerAttachFunction>();
    debugger_attach_function->set_extension(extension());
    EXPECT_FALSE(api_test_utils::RunFunction(
        debugger_attach_function.get(), target_in_other_profile.c_str(),
        profile(), api_test_utils::FunctionMode::kIncognito));
  }

  auto wc2 = CreateTabWithProfileAndNavigate(
      otr_profile(),
      embedded_test_server()->GetURL("/simple.html?off_the_record"));
  std::string target_in_otr_profile = base::StringPrintf(
      "[{\"targetId\": \"%s\"}, \"1.1\"]",
      content::DevToolsAgentHost::GetOrCreateFor(wc2.get())->GetId().c_str());

  {
    auto debugger_attach_function =
        base::MakeRefCounted<DebuggerAttachFunction>();
    debugger_attach_function->set_extension(extension());
    EXPECT_FALSE(api_test_utils::RunFunction(debugger_attach_function.get(),
                                             target_in_otr_profile.c_str(),
                                             profile()));
  }
  {
    auto debugger_attach_function =
        base::MakeRefCounted<DebuggerAttachFunction>();
    debugger_attach_function->set_extension(extension());
    EXPECT_TRUE(api_test_utils::RunFunction(
        debugger_attach_function.get(), target_in_otr_profile.c_str(),
        profile(), api_test_utils::FunctionMode::kIncognito));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       InfoBarIsNotRemovedIfAttachAgainBeforeFiveSeconds) {
  int tab_id =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(GetActiveWebContents());

  // Attaching to the tab should create an infobar.
  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  EXPECT_EQ(1u, manager->infobars().size());

  // Detaching from the tab and attaching it again before 5 seconds should not
  // remove the infobar.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      profile()));
  EXPECT_EQ(1u, manager->infobars().size());

  attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  // Verify that only one infobar is created.
  EXPECT_EQ(1u, manager->infobars().size());

  // Verify that infobar is not closed after 5 seconds.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
  AdvanceClock(ExtensionDevToolsInfoBarDelegate::kAutoCloseDelay);
  run_loop.Run();

  EXPECT_EQ(1u, manager->infobars().size());
}

// Tests that policy blocked hosts supersede the `debugger`
// permission. Regression test for crbug.com/40053634.
IN_PROC_BROWSER_TEST_F(DebuggerApiTest, TestDefaultPolicyBlockedHosts) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url("https://example.com/test");
  // The file does not exist so ignore navigation errors.
  EXPECT_TRUE(
      RunAttachFunction(url, std::string(), /*ignore_navigation_errors=*/true));
  URLPatternSet default_blocked_hosts;
  default_blocked_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTPS, "https://example.com/*"));
  PermissionsData::SetDefaultPolicyHostRestrictions(
      util::GetBrowserContextId(profile()), default_blocked_hosts,
      URLPatternSet());

  EXPECT_TRUE(RunAttachFunction(url, "Cannot attach to this target.",
                                /*ignore_navigation_errors=*/true));
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/441339825): Fails on desktop Android.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, Debugger) {
  ASSERT_TRUE(RunExtensionTest("debugger")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, DebuggerMv3) {
  ASSERT_TRUE(RunExtensionTest("debugger_mv3")) << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest,
                       FetchFulfillRequestCannotSetRestrictedCookie) {
  // Using HTTPS to allow testing secure http only cookies.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  GURL allowed_url = https_server.GetURL("a.test", "/index.html");
  GURL restricted_url = https_server.GetURL("b.test", "/index.html");

  URLPatternSet default_blocked_hosts;
  default_blocked_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL,
                 base::StringPrintf(
                     "*://%s/*", std::string(restricted_url.host()).c_str())));
  PermissionsData::SetDefaultPolicyHostRestrictions(
      util::GetBrowserContextId(profile()), default_blocked_hosts,
      URLPatternSet());

  std::string custom_arg = allowed_url.spec() + ";" + restricted_url.spec();
  ASSERT_TRUE(RunExtensionTest("debugger_fetch_cookie",
                               {.custom_arg = custom_arg.c_str()}))
      << message_;

  // We cannot verify the cookies from the extension because it would not
  // have access.
  base::test::TestFuture<const std::vector<net::CanonicalCookie>&>
      futureCookies;
  profile()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetAllCookies(futureCookies.GetCallback());
  bool found_restricted = false;
  bool found_allowed = false;
  for (const auto& cookie : futureCookies.Get()) {
    if (cookie.Name() == "restricted") {
      found_restricted = true;
    }
    if (cookie.Name() == "allowed") {
      found_allowed = true;
    }
  }
  EXPECT_FALSE(found_restricted) << "Restricted cookie was found";
  EXPECT_TRUE(found_allowed) << "Allowed cookie was not found";
}

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, ParentTargetPermissions) {
  // Run test with file access disabled.
  ASSERT_TRUE(RunExtensionTest("parent_target_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, ReloadAndResetHistory) {
  // Run test with file access disabled.
  ASSERT_TRUE(RunExtensionTest("debugger_reload_and_reset_history"))
      << message_;
}

// Tests that an extension is not allowed to inspect a worker through the
// inspectWorker debugger command.
// Regression test for https://crbug.com/40051715.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest,
                       DebuggerNotAllowedToInvokeInspectWorker) {
  GURL url(embedded_test_server()->GetURL(
      "/extensions/api_test/debugger_inspect_worker/inspected_page.html"));

  EXPECT_TRUE(RunExtensionTest("debugger_inspect_worker",
                               {.custom_arg = url.spec().c_str()}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, AttachToEmptyUrls) {
  ASSERT_TRUE(RunExtensionTest("debugger_attach_to_empty_urls")) << message_;
}

#if BUILDFLAG(ENABLE_PDF)
class DebuggerExtensionApiPdfTest : public base::test::WithFeatureOverride,
                                    public DebuggerExtensionApiTest {
 public:
  DebuggerExtensionApiPdfTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}
};

// Test that the debuggers can attach to the PDF embedder frame.
IN_PROC_BROWSER_TEST_P(DebuggerExtensionApiPdfTest, AttachToPdf) {
  ASSERT_TRUE(RunExtensionTest("debugger_attach_to_pdf")) << message_;
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(DebuggerExtensionApiPdfTest);

class DebuggerExtensionApiOopifPdfTest : public DebuggerExtensionApiTest {
 public:
  DebuggerExtensionApiOopifPdfTest() {
    feature_list_.InitAndEnableFeature(chrome_pdf::features::kPdfOopif);
  }

  pdf::TestMimeHandlerStreamManager* GetTestMimeHandlerStreamManager() {
    return factory_.GetTestMimeHandlerStreamManager(GetActiveWebContents());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  pdf::TestMimeHandlerStreamManagerFactory factory_;
};

// Test that the inner PDF frames, i.e. the PDF extension frame and the PDF
// content frame, aren't visible targets, while the PDF embedder frame is.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiOopifPdfTest, GetTargets) {
  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));

  // Load a full-page PDF.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(GetTestMimeHandlerStreamManager()->WaitUntilPdfLoaded(
      web_contents->GetPrimaryMainFrame()));

  // Get targets.
  base::ListValue targets = RunGetTargets(profile());

  // Verify that the inner PDF frames aren't targets in the list. Only the PDF
  // embedder frame (the main frame) should be a target.
  ASSERT_THAT(targets, testing::SizeIs(1));

  // Verify that the target is the PDF embedder frame.
  std::vector<std::string> urls = GetTargetUrlsWithoutPorts(targets);
  ASSERT_THAT(urls, testing::SizeIs(1));
  EXPECT_EQ(urls[0], "http://127.0.0.1/pdf/test.pdf");
}
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/441339825): Fails on desktop Android.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, AttachToBlob) {
  ASSERT_TRUE(RunExtensionTest("debugger_attach_to_blob_urls")) << message_;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Tests that navigation to a forbidden URL is properly denied and
// does not cause a crash.
// This is a regression test for https://crbug.com/40055226.
// TODO(crbug.com/41483732): Re-enable this test.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
#define MAYBE_NavigateToForbiddenUrl DISABLED_NavigateToForbiddenUrl
#else
#define MAYBE_NavigateToForbiddenUrl NavigateToForbiddenUrl
#endif
IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, MAYBE_NavigateToForbiddenUrl) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  ASSERT_TRUE(RunExtensionTest("debugger_navigate_to_forbidden_url"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, NavigateToUntrustedWebUIUrl) {
  ASSERT_TRUE(RunExtensionTest("debugger_navigate_to_untrusted_webui_url"))
      << message_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/441339825): Fails on desktop Android.
// Tests that Target.createTarget to WebUI origins are blocked.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionApiTest, CreateTargetToUntrustedWebUI) {
  ASSERT_TRUE(RunExtensionTest("debugger_create_target_to_untrusted_webui"))
      << message_;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class SitePerProcessDebuggerExtensionApiTest : public DebuggerExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DebuggerExtensionApiTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/441339825): Fails on desktop Android.
IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest, Debugger) {
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/extensions/api_test/debugger/oopif.html"));
  GURL iframe_url(embedded_test_server()->GetURL(
      "b.com", "/extensions/api_test/debugger/oopif_frame.html"));
  content::WebContents* tab = GetActiveWebContents();
  content::TestNavigationManager navigation_manager(tab, url);
  content::TestNavigationManager navigation_manager_iframe(tab, iframe_url);
  tab->GetController().LoadURL(url, content::Referrer(),
                               ui::PAGE_TRANSITION_LINK, std::string());
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  ASSERT_TRUE(navigation_manager_iframe.WaitForNavigationFinished());
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ASSERT_TRUE(RunExtensionTest("debugger",
                               {.custom_arg = "oopif.html;oopif_frame.html"}))
      << message_;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       NavigateSubframe) {
  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/extensions/api_test/debugger_navigate_subframe/inspected_page.html"));
  ASSERT_TRUE(RunExtensionTest("debugger_navigate_subframe",
                               {.custom_arg = url.spec().c_str()}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       NavigateSubframePolicyRestriction) {
  URLPatternSet default_blocked_hosts;
  default_blocked_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://c.com/*"));
  PermissionsData::SetDefaultPolicyHostRestrictions(
      util::GetBrowserContextId(profile()), default_blocked_hosts,
      URLPatternSet());

  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/extensions/api_test/debugger_navigate_subframe_policy_restriction/"
      "inspected_page.html"));
  ASSERT_TRUE(RunExtensionTest("debugger_navigate_subframe",
                               {.custom_arg = url.spec().c_str()}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       AutoAttachPermissions) {
  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/extensions/api_test/debugger_auto_attach_permissions/page.html"));
  ASSERT_TRUE(RunExtensionTest("debugger_auto_attach_permissions",
                               {.custom_arg = url.spec().c_str()}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       AutoAttachFlatModePermissions) {
  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/extensions/api_test/debugger_auto_attach_flat_mode_permissions/"
      "page.html"));
  ASSERT_TRUE(RunExtensionTest("debugger_auto_attach_flat_mode_permissions",
                               {.custom_arg = url.spec().c_str()}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       DebuggerCheckInnerUrl) {
  ASSERT_TRUE(RunExtensionTest("debugger_check_inner_url")) << message_;
}

IN_PROC_BROWSER_TEST_F(SitePerProcessDebuggerExtensionApiTest,
                       OopifAutoAttachWebAccessibleResourcesBypass) {
  TestExtensionDir victim_dir;
  victim_dir.WriteManifest(R"({
    "name": "Victim",
    "version": "1.0",
    "manifest_version": 3
  })");
  victim_dir.WriteFile(FILE_PATH_LITERAL("restricted.html"),
                       "<html>Restricted</html>");
  const Extension* victim = LoadExtension(victim_dir.UnpackedPath());
  ASSERT_TRUE(victim);

  GURL url(embedded_test_server()->GetURL(
      "a.com",
      "/extensions/api_test/debugger_oopif_auto_attach_war_bypass/page.html"));

  std::string custom_arg = url.spec() + ";" + victim->id();
  ASSERT_TRUE(RunExtensionTest("debugger_oopif_auto_attach_war_bypass",
                               {.custom_arg = custom_arg.c_str()}))
      << message_;
}

}  // namespace extensions
