// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/page_transition_types.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_manager.h"
#endif
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/extensions/profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_closed_waiter.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/permissions/permissions_updater.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "pdf/buildflags.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/base/base_window.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/test_event_router_observer.h"
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

#if BUILDFLAG(IS_ANDROID)
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#else
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/test_mime_handler_stream_manager.h"
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
  DebuggerApiTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kDisableExtensionsOnChromeUrlsSwitch);
  }
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
  base::test::ScopedFeatureList feature_list_;
};

void DebuggerApiTest::SetUpCommandLine(base::CommandLine* command_line) {
  ExtensionApiTest::SetUpCommandLine(command_line);
  // We need to hold onto |command_line| in order to modify it during the test.
  command_line_ = command_line;
}

class DebuggerFileAccessApiTest : public DebuggerApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DebuggerApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kAllowFileAccessFromFiles);
  }
};

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

using PwcDebuggerApiTest = DebuggerApiTest;

// An extension with the "debugger" permission cannot attach to, or even
// enumerate, a privileged WebContents (see //chrome's PrivilegedWebContents):
// it is invisible to extensions.
IN_PROC_BROWSER_TEST_F(PwcDebuggerApiTest,
                       CannotAttachToPrivilegedWebContents) {
  std::unique_ptr<pwc::PrivilegedWebContents> privileged =
      pwc::PrivilegedWebContents::Create(
          pwc::PrivilegedComponent::kTestComponent, profile(),
          std::make_unique<pwc::FixedPwcPolicyDelegate>(
              std::vector<url::Origin>{}, std::vector<url::Origin>{}));
  const std::string target_id =
      content::DevToolsAgentHost::GetOrCreateFor(privileged->web_contents())
          ->GetId();

  // chrome.debugger.getTargets does not list the privileged WebContents.
  scoped_refptr<DebuggerGetTargetsFunction> get_targets =
      base::MakeRefCounted<DebuggerGetTargetsFunction>();
  std::optional<base::Value> targets =
      api_test_utils::RunFunctionAndReturnSingleResult(get_targets.get(), "[]",
                                                       profile());
  ASSERT_TRUE(targets && targets->is_list());
  for (const base::Value& target : targets->GetList()) {
    const std::string* id = target.GetDict().FindString("id");
    EXPECT_FALSE(id && *id == target_id)
        << "A privileged WebContents must not be listed by getTargets";
  }

  // Attaching by the privileged WebContents' target id fails.
  scoped_refptr<DebuggerAttachFunction> attach =
      base::MakeRefCounted<DebuggerAttachFunction>();
  attach->set_extension(extension());
  EXPECT_FALSE(api_test_utils::RunFunction(
      attach.get(),
      base::StringPrintf(R"([{"targetId": "%s"}, "1.1"])", target_id.c_str()),
      profile()));
  EXPECT_EQ(manifest_errors::kCannotAccessPage, attach->GetError());
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
  EXPECT_TRUE(RunAttachFunction(other_ext_url,
                                manifest_errors::kCannotAccessExtensionUrl));

  // This extension *should* be able to debug itself.
  EXPECT_TRUE(RunAttachFunction(extension()->GetResourceURL("test_file.html"),
                                std::string()));

  // Append extensions on chrome urls switch. The extension should now be able
  // to debug any extension.
  command_line()->AppendSwitch(switches::kExtensionsOnChromeURLs);
  EXPECT_TRUE(RunAttachFunction(other_ext_url, std::string()));
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerCannotClearOtherExtensionStorage) {
  base::FilePath path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").AppendASCII("simple_with_popup");
  const Extension* another_extension = LoadExtension(path);
  ASSERT_TRUE(another_extension);

  GURL regular_url = embedded_test_server()->GetURL("/simple.html");
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, regular_url));

  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf(R"([{"tabId": %d}, "1.3"])", tab_id), profile()))
      << attach_function->GetError();

  std::string other_ext_origin = another_extension->origin().Serialize();
  std::string other_ext_storage_key = other_ext_origin + "/";

  auto clear_origin_cmd = base::MakeRefCounted<DebuggerSendCommandFunction>();
  clear_origin_cmd->set_extension(extension());
  std::string clear_origin_args =
      base::StringPrintf(R"([{"tabId": %d}, "Storage.clearDataForOrigin", )"
                         R"({"origin": "%s", "storageTypes": "all"}])",
                         tab_id, other_ext_origin.c_str());
  std::string origin_error = api_test_utils::RunFunctionAndReturnError(
      clear_origin_cmd.get(), clear_origin_args, profile());
  EXPECT_THAT(origin_error, testing::HasSubstr("Permission denied"));

  auto clear_key_cmd = base::MakeRefCounted<DebuggerSendCommandFunction>();
  clear_key_cmd->set_extension(extension());
  std::string clear_key_args =
      base::StringPrintf(R"([{"tabId": %d}, "Storage.clearDataForStorageKey", )"
                         R"({"storageKey": "%s", "storageTypes": "all"}])",
                         tab_id, other_ext_storage_key.c_str());
  std::string key_error = api_test_utils::RunFunctionAndReturnError(
      clear_key_cmd.get(), clear_key_args, profile());
  EXPECT_THAT(key_error, testing::HasSubstr("Permission denied"));

  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(extension());
  EXPECT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf(R"([{"tabId": %d}])", tab_id),
      profile()));
}

IN_PROC_BROWSER_TEST_F(DebuggerFileAccessApiTest,
                       DebuggerAllowedOnFileUrlsWithFileAccess) {
  base::FilePath worker_path =
      test_data_dir_.AppendASCII("debugger_file_access")
          .AppendASCII("worker.html");
  GURL worker_url = net::FilePathToFileURL(worker_path);
  std::string custom_arg = worker_url.spec() + "|enabled";
  EXPECT_TRUE(RunExtensionTest("debugger_file_access",
                               {.custom_arg = custom_arg.c_str()},
                               {.allow_file_access = true}))
      << message_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// TODO(crbug.com/441339825): Fails on desktop Android with an error about
// access to localhost.
IN_PROC_BROWSER_TEST_F(DebuggerFileAccessApiTest,
                       DebuggerNotAllowedOnFileUrlsWithoutAccess) {
  base::FilePath worker_path =
      test_data_dir_.AppendASCII("debugger_file_access")
          .AppendASCII("worker.html");
  GURL worker_url = net::FilePathToFileURL(worker_path);
  std::string custom_arg = worker_url.spec() + "|disabled";

  EXPECT_TRUE(RunExtensionTest("debugger_file_access",
                               {.custom_arg = custom_arg.c_str()}))
      << message_;
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
                std::string(base::i18n::GetDefaultIcuLocale().tag_string()),
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
  EXPECT_TRUE(RunAttachFunction(blob_web_contents,
                                "Host access is restricted by policy."));
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

#if BUILDFLAG(IS_ANDROID)
// Android uses messages for warnings instead of infobars.
IN_PROC_BROWSER_TEST_F(DebuggerApiTest, Messages) {
  int tab_id =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();
  scoped_refptr<DebuggerAttachFunction> attach_function;
  scoped_refptr<DebuggerDetachFunction> detach_function;

  messages::MockMessageDispatcherBridge mock_bridge;
  mock_bridge.SetMessagesEnabledForEmbedder(true);
  messages::MessageDispatcherBridge::SetInstanceForTesting(&mock_bridge);

  // Set up mocks to capture the message and handle cleanup.
  messages::MessageWrapper* captured_message = nullptr;
  EXPECT_CALL(mock_bridge,
              EnqueueWindowScopedMessage(testing::_, testing::_,
                                         messages::MessagePriority::kUrgent))
      .WillRepeatedly([&captured_message](messages::MessageWrapper* message,
                                          ui::WindowAndroid* window,
                                          messages::MessagePriority priority) {
        captured_message = message;
        message->SetMessageEnqueued(window->GetJavaObject());
        return true;
      });
  EXPECT_CALL(
      mock_bridge,
      DismissMessage(testing::_, messages::DismissReason::DISMISSED_BY_FEATURE))
      .WillRepeatedly([&captured_message](
                          messages::MessageWrapper* message,
                          messages::DismissReason dismiss_reason) {
        JNIEnv* env = base::android::AttachCurrentThread();
        message->HandleDismissCallback(env, static_cast<int>(dismiss_reason));
        captured_message = nullptr;
      });

  // Attaching should create a message.
  attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  ASSERT_NE(nullptr, captured_message);
  EXPECT_TRUE(captured_message->is_in_queue());

  // Detaching removes the message.
  detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      profile()));
  EXPECT_FALSE(captured_message);

  // Attach again; should create another message.
  attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  EXPECT_TRUE(captured_message->is_in_queue());

  // Simulating what happens when the user clicks the cancel button. The
  // extension is detached.
  JNIEnv* env = base::android::AttachCurrentThread();
  captured_message->HandleActionClick(env);

  // The message is closed.
  EXPECT_FALSE(captured_message);

  // Trying to detach again will fail because the extension is already detached.
  detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(extension());
  ASSERT_FALSE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id),
      profile()));

  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
}

#else  // BUILDFLAG(IS_ANDROID)

// Win/Mac/Linux/Chrome OS use infobars for warnings.
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

  // The infobar framework shows a global infobar only in the
  // active tab of each browser window, moving it as the active tab changes.
  // The legacy GlobalConfirmInfoBar instead puts an infobar in every tab. Only
  // `manager2` is a background tab (browser2's active tab is `manager3`), so
  // it is the only expectation that differs between the two implementations.
  const bool global_infobar_in_active_tab_only = infobars::IsInfoBarMigrated(
      infobars::InfoBarDelegate::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE);
  const size_t background_tab_infobars =
      global_infobar_in_active_tab_only ? 0u : 1u;
  // The infobar the user would click the close button on must be one that is
  // actually showing.
  infobars::ContentInfoBarManager* dismissal_manager =
      global_infobar_in_active_tab_only ? manager3 : manager2;

  // Attaching to one tab should create infobars in both browsers.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(background_tab_infobars, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Attaching to another tab should not create more infobars.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id2), profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(background_tab_infobars, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Detaching from one of the tabs should not remove infobars.
  detach_function = new DebuggerDetachFunction();
  detach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf("[{\"tabId\": %d}]", tab_id2),
      profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(background_tab_infobars, manager2->infobars().size());
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
  EXPECT_EQ(background_tab_infobars, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Attach again; should not create infobars.
  attach_function = new DebuggerAttachFunction();
  attach_function->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id), profile()));
  EXPECT_EQ(1u, manager1->infobars().size());
  EXPECT_EQ(background_tab_infobars, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Remove the global infobar by simulating what happens when the user clicks
  // the close button (see InfoBarView::ButtonPressed()).  The
  // InfoBarDismissed() call will remove the infobars everywhere except on
  // |dismissal_manager| itself; the RemoveSelf() call removes that one.
  dismissal_manager->infobars()[0]->delegate()->InfoBarDismissed();
  dismissal_manager->infobars()[0]->RemoveSelf();
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
  EXPECT_EQ(background_tab_infobars, manager2->infobars().size());
  EXPECT_EQ(1u, manager3->infobars().size());

  // Closing tab should not affect anything. Note that under the infobar
  // framework this makes `manager2`'s tab active, so the global
  // infobar moves there.
  EXPECT_EQ(2, tab_list2->GetTabCount());
  tab_list2->CloseTab(tab_list2->GetTab(1)->GetHandle());
  EXPECT_EQ(1, tab_list2->GetTabCount());
  manager3 = nullptr;
  dismissal_manager = nullptr;
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

  ASSERT_TRUE(embedded_test_server()->Started());
  ASSERT_TRUE(
      NavigateToURLInNewTab(embedded_test_server()->GetURL("/simple.html")));
  const int tab_id2 =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();

  // Watch the active tab's manager: the infobar framework only
  // shows a global infobar in the active tab of each browser window, while the
  // legacy GlobalConfirmInfoBar shows one in every tab. The active tab has an
  // infobar either way.
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(GetActiveWebContents());

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

// Regression test for https://crbug.com/514078071: truncating a long extension
// name must not strip the PDF that terminates a directional override in it,
// which would otherwise visually reverse the rest of the infobar message.
IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       InfoBarTerminatesDirectionalFormattingInLongName) {
  // Put an RLO before the 1000 character truncation point, and make the name
  // long enough that the PDF appended by name sanitization is truncated away.
  const std::u16string name = std::u16string(980, u'A') +
                              base::i18n::kRightToLeftOverride +
                              u".won efas si emorhC" + std::u16string(40, u'B');
  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      R"({
           "name": "%s",
           "version": "0.1",
           "manifest_version": 2,
           "permissions": ["debugger"]
         })",
      base::UTF16ToUTF8(name).c_str()));
  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const int tab_id =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();
  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension);
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf(R"([{"tabId": %d}, "1.1"])", tab_id), profile()));

  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(GetActiveWebContents());
  ASSERT_EQ(1u, manager->infobars().size());
  ConfirmInfoBarDelegate* delegate =
      manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  const std::u16string message = delegate->GetMessageText();

  // Split the message into the part containing the (truncated) extension name
  // and the trailing, security-relevant part of the message template.
  size_t name_offset = 0;
  const std::u16string message_template = l10n_util::GetStringFUTF16(
      IDS_DEV_TOOLS_INFOBAR_LABEL, std::u16string(), &name_offset);
  const std::u16string suffix = message_template.substr(name_offset);
  ASSERT_FALSE(suffix.empty());
  ASSERT_TRUE(message.ends_with(suffix)) << base::UTF16ToUTF8(message);
  ASSERT_NE(std::u16string::npos,
            message.find(base::i18n::kRightToLeftOverride));

  // All directional formatting must be terminated before the suffix.
  const std::u16string prefix =
      message.substr(0, message.size() - suffix.size());
  std::u16string terminated_prefix = prefix;
  base::i18n::EnsureTerminatedDirectionalFormatting(&terminated_prefix);
  EXPECT_EQ(prefix, terminated_prefix);
}

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
#endif  // BUILDFLAG(IS_ANDROID)

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

  EXPECT_TRUE(RunAttachFunction(url, "Host access is restricted by policy.",
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
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {
    if (IsParamFeatureEnabled()) {
      factory_.emplace();
    }
  }

  testing::AssertionResult LoadPdf(const GURL& pdf_url) {
    if (!ui_test_utils::NavigateToURL(browser(), pdf_url)) {
      return testing::AssertionFailure() << "NavigateToURL failed";
    }
    content::WebContents* web_contents = GetActiveWebContents();
    if (IsParamFeatureEnabled()) {
      return factory_->GetTestMimeHandlerStreamManager(web_contents)
          ->WaitUntilPdfLoaded(web_contents->GetPrimaryMainFrame());
    }
    return pdf_extension_test_util::EnsurePDFHasLoaded(web_contents);
  }

 private:
  std::optional<pdf::TestMimeHandlerStreamManagerFactory> factory_;
};

// Test that the debuggers can attach to the PDF embedder frame.
IN_PROC_BROWSER_TEST_P(DebuggerExtensionApiPdfTest, AttachToPdf) {
  ASSERT_TRUE(RunExtensionTest("debugger_attach_to_pdf")) << message_;
}

// Test that extensions cannot attach directly or via Target.setAutoAttach to
// the inner PDF extension frame or PDF content frame, while still being able to
// attach to the outer PDF embedder tab.
IN_PROC_BROWSER_TEST_P(DebuggerExtensionApiPdfTest,
                       CannotAttachToInnerPdfFrames) {
  TestExtensionDir test_extension_dir;
  test_extension_dir.WriteManifest(
      R"({
         "name": "debugger",
         "version": "0.1",
         "manifest_version": 3,
         "permissions": ["debugger"]
       })");
  const Extension* extension = LoadExtension(test_extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ASSERT_TRUE(LoadPdf(pdf_url));

  scoped_refptr<content::DevToolsAgentHost> pdf_extension_host;
  scoped_refptr<content::DevToolsAgentHost> pdf_content_host;
  for (const auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetURL().SchemeIs(extensions::kExtensionScheme) &&
        host->GetURL().GetHost() == extension_misc::kPdfExtensionId) {
      pdf_extension_host = host;
    } else if (host->GetProcessHost() && host->GetProcessHost()->IsPdf()) {
      pdf_content_host = host;
    }
  }
  ASSERT_TRUE(pdf_extension_host);
  ASSERT_TRUE(pdf_content_host);

  const std::string expected_direct_attach_error =
      IsParamFeatureEnabled() ? "Cannot attach to this target."
                              : manifest_errors::kCannotAccessExtensionUrl;

  // Direct attachment to the PDF extension frame must be rejected.
  {
    auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
    attach_function->set_extension(extension);
    EXPECT_FALSE(api_test_utils::RunFunction(
        attach_function.get(),
        base::StringPrintf(R"([{"targetId": "%s"}, "1.3"])",
                           pdf_extension_host->GetId().c_str()),
        profile()));
    EXPECT_EQ(expected_direct_attach_error, attach_function->GetError());
    EXPECT_FALSE(pdf_extension_host->IsAttached());
  }

  // Direct attachment to the PDF content frame must be rejected.
  {
    auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
    attach_function->set_extension(extension);
    EXPECT_FALSE(api_test_utils::RunFunction(
        attach_function.get(),
        base::StringPrintf(R"([{"targetId": "%s"}, "1.3"])",
                           pdf_content_host->GetId().c_str()),
        profile()));
    EXPECT_EQ(expected_direct_attach_error, attach_function->GetError());
    EXPECT_FALSE(pdf_content_host->IsAttached());
  }

  // Attaching to the outer PDF embedder tab succeeds, and neither
  // Target.setAutoAttach nor Target.attachToTarget may attach to the inner PDF
  // extension or content frames.
  int tab_id =
      sessions::SessionTabHelper::IdForTab(GetActiveWebContents()).id();
  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(extension);
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_function.get(),
      base::StringPrintf(R"([{"tabId": %d}, "1.3"])", tab_id), profile()));

  auto send_command = base::MakeRefCounted<DebuggerSendCommandFunction>();
  send_command->set_extension(extension);
  ASSERT_TRUE(api_test_utils::RunFunction(
      send_command.get(),
      base::StringPrintf(
          R"([{"tabId": %d}, "Target.setAutoAttach", )"
          R"({"autoAttach": true, "waitForDebuggerOnStart": false, )"
          R"("flatten": true}])",
          tab_id),
      profile()));

  EXPECT_FALSE(pdf_extension_host->IsAttached());
  EXPECT_FALSE(pdf_content_host->IsAttached());

  for (const auto& restricted_host : {pdf_extension_host, pdf_content_host}) {
    auto attach_to_target = base::MakeRefCounted<DebuggerSendCommandFunction>();
    attach_to_target->set_extension(extension);
    std::string error = api_test_utils::RunFunctionAndReturnError(
        attach_to_target.get(),
        base::StringPrintf(
            R"([{"tabId": %d}, "Target.attachToTarget", {"targetId": "%s"}])",
            tab_id, restricted_host->GetId().c_str()),
        profile());
    EXPECT_THAT(error, testing::HasSubstr("Not allowed"));
    EXPECT_FALSE(restricted_host->IsAttached());
  }

  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(extension);
  EXPECT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), base::StringPrintf(R"([{"tabId": %d}])", tab_id),
      profile()));
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

// Test fixture for verifying chrome.debugger screenshot policy behavior when
// the kExtensionDebuggerStrictPolicyRestrictions feature flag is explicitly
// enabled (crbug.com/561948316).
class SitePerProcessDebuggerExtensionApiStrictPolicyEnabledTest
    : public SitePerProcessDebuggerExtensionApiTest {
 public:
  SitePerProcessDebuggerExtensionApiStrictPolicyEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionDebuggerStrictPolicyRestrictions);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SitePerProcessDebuggerExtensionApiStrictPolicyEnabledTest,
    AttachRejectedWhenScreenshotsDisabled) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots, true);

  ASSERT_TRUE(RunExtensionTest(
      "debugger_disable_screenshots",
      {.custom_arg = "Screenshot capture is restricted by policy."}))
      << message_;
}

#if BUILDFLAG(IS_CHROMEOS)
// Target-level screenshot restrictions via Data Leak Prevention (DLP) are
// currently only supported on ChromeOS.
IN_PROC_BROWSER_TEST_F(
    SitePerProcessDebuggerExtensionApiStrictPolicyEnabledTest,
    AttachRejectedWhenScreenshotsRestrictedByDlp) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager,
              IsScreenshotApiRestricted(web_contents()))
      .WillOnce(testing::Return(true));

  ASSERT_TRUE(RunExtensionTest(
      "debugger_disable_screenshots",
      {.custom_arg = "Screenshot capture is restricted on this target."}))
      << message_;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Tests that a trusted component extension is exempt from global screenshot
// restrictions and can attach to the browser target even when enterprise
// policy disables screenshots.
IN_PROC_BROWSER_TEST_F(
    SitePerProcessDebuggerExtensionApiStrictPolicyEnabledTest,
    BrowserTargetAllowedForTrustedExtensionWhenScreenshotsDisabled) {
  profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots, true);

  scoped_refptr<const Extension> component_trusted_extension =
      ExtensionBuilder("Some Trusted Extension")
          .SetID(extension_misc::kPerfettoUIExtensionId)
          .SetLocation(mojom::ManifestLocation::kComponent)
          .AddAPIPermission("debugger")
          .Build();
  PermissionsUpdater(profile()).InitializePermissions(
      component_trusted_extension.get());

  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(component_trusted_extension.get());

  EXPECT_TRUE(api_test_utils::RunFunction(
      attach_function.get(), R"([{"targetId": "browser"}, "1.1"])", profile()))
      << attach_function->GetError();

  // Clean up and detach.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(component_trusted_extension.get());
  EXPECT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), R"([{"targetId": "browser"}])", profile()));
}

class DebuggerExtensionManagementPolicyTestBase
    : public ExtensionApiTestWithManagementPolicy {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTestWithManagementPolicy::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }
};

// Test fixture for verifying chrome.debugger runtime_blocked_hosts policy
// behavior when the kExtensionDebuggerStrictPolicyRestrictions feature flag is
// explicitly enabled (crbug.com/561948316).
class DebuggerExtensionManagementPolicyTest
    : public DebuggerExtensionManagementPolicyTestBase {
 public:
  DebuggerExtensionManagementPolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionDebuggerStrictPolicyRestrictions);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that attaching the debugger to a blocked host is rejected when
// runtime blocked hosts are configured globally via enterprise policy (*).
IN_PROC_BROWSER_TEST_F(DebuggerExtensionManagementPolicyTest,
                       AttachRejectedWithPolicyBlockedHostsGlobal_BlockedHost) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  // Set up runtime blocked hosts globally for all extensions.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  // Attempting to attach to a blocked host fails with policy error.
  ASSERT_TRUE(
      RunExtensionTest("debugger_policy_blocked_hosts",
                       {.custom_arg = "Host access is restricted by policy."}))
      << message_;
}

// Tests that under the all-or-nothing model, attaching the debugger to an
// unblocked host is also rejected when runtime blocked hosts are configured
// globally via enterprise policy (*).
IN_PROC_BROWSER_TEST_F(
    DebuggerExtensionManagementPolicyTest,
    AttachRejectedWithPolicyBlockedHostsGlobal_UnblockedHost) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL("b.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  // Set up runtime blocked hosts globally for all extensions (blocking a.test).
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  // Attempting to attach to an unblocked host (b.test) also fails.
  ASSERT_TRUE(
      RunExtensionTest("debugger_policy_blocked_hosts",
                       {.custom_arg = "Host access is restricted by policy."}))
      << message_;
}

// Tests that attaching the debugger is rejected when runtime blocked hosts are
// configured specifically for this extension.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionManagementPolicyTest,
                       AttachRejectedWithPolicyBlockedHostsPerExtension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  ExtensionId extension_id = crx_file::id_util::GenerateIdForPath(
      test_data_dir_.AppendASCII("debugger_policy_blocked_hosts"));
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost(extension_id, "*://a.test");
  }

  ASSERT_TRUE(
      RunExtensionTest("debugger_policy_blocked_hosts",
                       {.custom_arg = "Host access is restricted by policy."}))
      << message_;
}

// Tests that an extension can attach when it explicitly overrides the global
// blocked hosts policy with empty blocked hosts.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionManagementPolicyTest,
                       AttachAllowedWhenPolicyBlockedHostsOverridden) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  ExtensionId extension_id = crx_file::id_util::GenerateIdForPath(
      test_data_dir_.AppendASCII("debugger_policy_blocked_hosts"));
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
    pref.ClearPolicyBlockedHosts(extension_id);
  }

  ASSERT_TRUE(RunExtensionTest("debugger_policy_blocked_hosts")) << message_;
}

// Tests that when enterprise policy uses runtime_blocked_hosts and
// runtime_allowed_hosts to configure an allowlist of URLs, attaching the
// debugger is still rejected even on an allowlisted host.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionManagementPolicyTest,
                       AttachRejectedWithPolicyAllowlist) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  // Set up an allowlist: block all hosts, but allow a.test.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*");
    pref.AddPolicyAllowedHost("*", "*://a.test");
  }

  // Under the all-or-nothing model, attaching is rejected even on an
  // allowlisted host.
  ASSERT_TRUE(
      RunExtensionTest("debugger_policy_blocked_hosts",
                       {.custom_arg = "Host access is restricted by policy."}))
      << message_;
}

// Tests that an extension can attach normally when no runtime blocked hosts
// are configured by enterprise policy.
IN_PROC_BROWSER_TEST_F(DebuggerExtensionManagementPolicyTest,
                       AttachAllowedWithoutPolicyBlockedHosts) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  ASSERT_TRUE(RunExtensionTest("debugger_policy_blocked_hosts")) << message_;
}

// Tests that a trusted component extension is exempt from the
// policy_blocked_hosts check and can attach to the browser target even when
// runtime blocked hosts are configured globally via enterprise policy (*).
IN_PROC_BROWSER_TEST_F(
    DebuggerExtensionManagementPolicyTest,
    BrowserTargetAllowedForComponentTrustedExtensionWithPolicyBlockedHosts) {
  // Set up runtime blocked hosts globally for all extensions.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*");
  }

  scoped_refptr<const Extension> component_trusted_extension =
      ExtensionBuilder("Some Trusted Extension")
          .SetID(extension_misc::kPerfettoUIExtensionId)
          .SetLocation(mojom::ManifestLocation::kComponent)
          .AddAPIPermission("debugger")
          .Build();
  PermissionsUpdater(profile()).InitializePermissions(
      component_trusted_extension.get());
  // Verify that policy blocked hosts were populated from enterprise policy.
  ASSERT_FALSE(component_trusted_extension->permissions_data()
                   ->policy_blocked_hosts()
                   .is_empty());

  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(component_trusted_extension.get());

  EXPECT_TRUE(api_test_utils::RunFunction(
      attach_function.get(), R"([{"targetId": "browser"}, "1.1"])", profile()))
      << attach_function->GetError();

  // Clean up and detach.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(component_trusted_extension.get());
  EXPECT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), R"([{"targetId": "browser"}])", profile()));
}

// Tests that an unpacked trusted extension is exempt from the
// policy_blocked_hosts check when the allow unpacked switch is set.
IN_PROC_BROWSER_TEST_F(
    DebuggerExtensionManagementPolicyTest,
    BrowserTargetAllowedForUnpackedTrustedExtensionWithFlagAndPolicyBlockedHosts) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kAllowUnpackedPerfettoExtension);

  // Set up runtime blocked hosts globally for all extensions.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*");
  }

  scoped_refptr<const Extension> unpacked_trusted_extension =
      ExtensionBuilder("Some Trusted Extension")
          .SetID(extension_misc::kPerfettoUIExtensionId)
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .AddAPIPermission("debugger")
          .Build();
  PermissionsUpdater(profile()).InitializePermissions(
      unpacked_trusted_extension.get());
  // Verify that policy blocked hosts were populated from enterprise policy.
  ASSERT_FALSE(unpacked_trusted_extension->permissions_data()
                   ->policy_blocked_hosts()
                   .is_empty());

  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(unpacked_trusted_extension.get());

  EXPECT_TRUE(api_test_utils::RunFunction(
      attach_function.get(), R"([{"targetId": "browser"}, "1.1"])", profile()))
      << attach_function->GetError();

  // Clean up and detach.
  auto detach_function = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_function->set_extension(unpacked_trusted_extension.get());
  EXPECT_TRUE(api_test_utils::RunFunction(
      detach_function.get(), R"([{"targetId": "browser"}])", profile()));
}

// Tests that an unpacked extension without the command line flag is not
// treated as trusted and cannot attach to the browser target when policy
// blocked hosts are configured.
IN_PROC_BROWSER_TEST_F(
    DebuggerExtensionManagementPolicyTest,
    BrowserTargetNotAllowedForUnpackedTrustedExtensionWithPolicyBlockedHosts) {
  // Set up runtime blocked hosts globally for all extensions.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://*");
  }

  scoped_refptr<const Extension> unpacked_trusted_extension =
      ExtensionBuilder("Some Trusted Extension")
          .SetID(extension_misc::kPerfettoUIExtensionId)
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .AddAPIPermission("debugger")
          .Build();
  PermissionsUpdater(profile()).InitializePermissions(
      unpacked_trusted_extension.get());
  // Verify that policy blocked hosts were populated from enterprise policy.
  ASSERT_FALSE(unpacked_trusted_extension->permissions_data()
                   ->policy_blocked_hosts()
                   .is_empty());

  auto attach_function = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_function->set_extension(unpacked_trusted_extension.get());

  std::string actual_error = api_test_utils::RunFunctionAndReturnError(
      attach_function.get(), R"([{"targetId": "browser"}, "1.1"])", profile());

  EXPECT_EQ("Host access is restricted by policy.", actual_error);
}

// Test fixture for verifying chrome.debugger screenshot policy behavior when
// the kExtensionDebuggerStrictPolicyRestrictions feature flag is disabled
// (crbug.com/561948316).
class SitePerProcessDebuggerExtensionApiStrictPolicyDisabledTest
    : public SitePerProcessDebuggerExtensionApiTest {
 public:
  SitePerProcessDebuggerExtensionApiStrictPolicyDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionDebuggerStrictPolicyRestrictions);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that when kExtensionDebuggerStrictPolicyRestrictions is disabled
// (crbug.com/561948316), attaching the debugger is allowed even if enterprise
// policy disables screenshots globally.
IN_PROC_BROWSER_TEST_F(
    SitePerProcessDebuggerExtensionApiStrictPolicyDisabledTest,
    AttachAllowedWhenScreenshotsDisabled) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots, true);

  ASSERT_TRUE(RunExtensionTest("debugger_disable_screenshots",
                               {.custom_arg = "debuggerAllowed"}))
      << message_;
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests that when kExtensionDebuggerStrictPolicyRestrictions is disabled
// (crbug.com/561948316), attaching the debugger is allowed even if DLP
// restricts screenshots on the target.
IN_PROC_BROWSER_TEST_F(
    SitePerProcessDebuggerExtensionApiStrictPolicyDisabledTest,
    AttachAllowedWhenScreenshotsRestrictedByDlp) {
  ASSERT_TRUE(embedded_test_server()->Started());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager,
              IsScreenshotApiRestricted(web_contents()))
      .WillRepeatedly(testing::Return(true));

  ASSERT_TRUE(RunExtensionTest("debugger_disable_screenshots",
                               {.custom_arg = "debuggerAllowed"}))
      << message_;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Test fixture for verifying chrome.debugger runtime_blocked_hosts policy
// behavior when the kExtensionDebuggerStrictPolicyRestrictions feature flag is
// disabled (crbug.com/561948316).
class DebuggerExtensionManagementPolicyStrictPolicyDisabledTest
    : public DebuggerExtensionManagementPolicyTestBase {
 public:
  DebuggerExtensionManagementPolicyStrictPolicyDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionDebuggerStrictPolicyRestrictions);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that when kExtensionDebuggerStrictPolicyRestrictions is disabled
// (crbug.com/561948316), attaching the debugger to an unblocked host succeeds
// even when enterprise policy blocks another host (restoring legacy per-URL
// behavior).
IN_PROC_BROWSER_TEST_F(
    DebuggerExtensionManagementPolicyStrictPolicyDisabledTest,
    AttachAllowedWithPolicyBlockedHostsGlobal_UnblockedHost) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL("b.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  // Set up runtime blocked hosts globally for all extensions (blocking a.test).
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  // Attempting to attach to an unblocked host (b.test) succeeds.
  ASSERT_TRUE(RunExtensionTest("debugger_policy_blocked_hosts")) << message_;
}

// Tests that when kExtensionDebuggerStrictPolicyRestrictions is disabled
// (crbug.com/561948316), attaching the debugger directly to a policy-blocked
// host is still rejected with the legacy per-URL policy error.
IN_PROC_BROWSER_TEST_F(
    DebuggerExtensionManagementPolicyStrictPolicyDisabledTest,
    AttachRejectedWithPolicyBlockedHostsGlobal_BlockedHost) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL url = embedded_test_server()->GetURL("a.test", "/english_page.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  // Set up runtime blocked hosts globally for all extensions (blocking a.test).
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  // Attempting to attach to the blocked host (a.test) fails with the legacy
  // per-URL error message.
  ASSERT_TRUE(RunExtensionTest(
      "debugger_policy_blocked_hosts",
      {.custom_arg =
           "This page cannot be scripted due to an ExtensionsSettings policy."}))
      << message_;
}

#if BUILDFLAG(ENABLE_GUEST_VIEW)

namespace {

// Allow-listed WebUI for <webview> in chrome/.../_api_features.json.
constexpr const char* kTestWebUIHost = chrome::kChromeUIChromeSigninHost;
constexpr const char* kTestWebUIURL = chrome::kChromeUIChromeSigninURL;

// A custom WebUIController hosting a <webview> to test debugger restrictions.
class TestWebUIController : public content::WebUIController {
 public:
  explicit TestWebUIController(content::WebUI* web_ui)
      : content::WebUIController(web_ui) {
    web_ui->SetBindings(
        content::BindingsPolicySet({content::BindingsPolicyValue::kWebUi}));
    content::WebContents* wc = web_ui->GetWebContents();
    extensions::TabHelper::CreateForWebContents(wc);
    content::WebUIDataSource* s = content::WebUIDataSource::CreateAndAdd(
        wc->GetBrowserContext(), kTestWebUIHost);
    s->SetRequestFilter(
        base::BindRepeating([](const std::string& p) { return p.empty(); }),
        base::BindRepeating(
            [](const std::string&,
               content::WebUIDataSource::GotDataCallback cb) {
              std::move(cb).Run(base::MakeRefCounted<base::RefCountedString>(
                  "<!DOCTYPE html><webview src=\"about:blank\"></webview>"));
            }));
  }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIController)

class TestWebUIConfig
    : public content::DefaultWebUIConfig<TestWebUIController> {
 public:
  TestWebUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, kTestWebUIHost) {}
};

class TestGuestViewManagerDelegate
    : public extensions::ExtensionsGuestViewManagerDelegate {
 public:
  bool IsGuestAvailableToContext(const guest_view::GuestViewBase*)
      const override {
    return true;
  }
};

}  // namespace

// Tests chrome.debugger access to <webview> guests inside privileged WebUI.
class WebViewGuestDebuggerApiTest : public DebuggerApiTest,
                                    public testing::WithParamInterface<bool> {
 public:
  WebViewGuestDebuggerApiTest() {
    guest_view_feature_list_.InitWithFeatureState(features::kGuestViewMPArch,
                                                  GetParam());
  }

  void SetUpOnMainThread() override {
    DebuggerApiTest::SetUpOnMainThread();
    web_ui_config_registration_ =
        std::make_unique<content::ScopedWebUIConfigRegistration>(
            std::make_unique<TestWebUIConfig>());
  }

 protected:
  guest_view::TestGuestViewManager* GetGuestViewManager() {
    return factory_.GetOrCreateTestGuestViewManager(
        profile(), std::make_unique<TestGuestViewManagerDelegate>());
  }

  scoped_refptr<content::DevToolsAgentHost> GetGuestAgentHost() {
    auto hosts = content::DevToolsAgentHost::GetOrCreateAll();
    auto it = std::ranges::find(hosts, content::DevToolsAgentHost::kTypeGuest,
                                &content::DevToolsAgentHost::GetType);
    return it != hosts.end() ? *it : nullptr;
  }

  void CreateWebUiGuestView() {
    auto* manager = GetGuestViewManager();
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), GURL(kTestWebUIURL)));
    auto* view = manager->WaitForSingleGuestViewCreated();
    ASSERT_TRUE(view && manager->WaitUntilAttachedAndLoaded(view));
  }

  bool HasTargetId(const std::string& target_id) {
    auto fn = base::MakeRefCounted<DebuggerGetTargetsFunction>();
    fn->set_extension(extension());
    auto val = api_test_utils::RunFunctionAndReturnSingleResult(fn.get(), "[]",
                                                                profile());
    return val && val->is_list() &&
           std::ranges::any_of(val->GetList(), [&](const auto& item) {
             const std::string* id = item.GetDict().FindString("id");
             return id && *id == target_id;
           });
  }

 private:
  base::test::ScopedFeatureList guest_view_feature_list_;
  guest_view::TestGuestViewManagerFactory factory_;
  std::unique_ptr<content::ScopedWebUIConfigRegistration>
      web_ui_config_registration_;
};

// Tests that attach() refuses to attach to a WebUI-hosted <webview> guest.
IN_PROC_BROWSER_TEST_P(WebViewGuestDebuggerApiTest,
                       CannotAttachToWebUiWebViewGuest) {
  CreateWebUiGuestView();
  auto attach = base::MakeRefCounted<DebuggerAttachFunction>();
  attach->set_extension(extension());
  EXPECT_EQ(manifest_errors::kCannotAccessChromeUrl,
            api_test_utils::RunFunctionAndReturnError(
                attach.get(),
                base::StringPrintf(R"([{"targetId": "%s"}, "1.1"])",
                                   GetGuestAgentHost()->GetId().c_str()),
                profile()));
}

// Tests that chrome.debugger.getTargets() omits WebUI <webview> guests.
IN_PROC_BROWSER_TEST_P(WebViewGuestDebuggerApiTest,
                       WebUiWebViewGuestExcludedFromGetTargets) {
  CreateWebUiGuestView();
  EXPECT_FALSE(HasTargetId(GetGuestAgentHost()->GetId()));
}

// Verifies attached session cannot reach WebUI guest via Target.attachToTarget.
IN_PROC_BROWSER_TEST_P(WebViewGuestDebuggerApiTest,
                       AlreadyAttachedSessionCannotReachWebUiWebViewGuest) {
  CreateWebUiGuestView();
  auto* new_wc = content::WebContents::FromRenderFrameHost(
      NavigateToURLInNewTab(GURL("about:blank")));
  int tab_id = sessions::SessionTabHelper::IdForTab(new_wc).id();

  auto attach = base::MakeRefCounted<DebuggerAttachFunction>();
  attach->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach.get(), base::StringPrintf(R"([{"tabId": %d}, "1.3"])", tab_id),
      profile()));

  auto send_cmd = base::MakeRefCounted<DebuggerSendCommandFunction>();
  send_cmd->set_extension(extension());
  EXPECT_THAT(
      api_test_utils::RunFunctionAndReturnError(
          send_cmd.get(),
          base::StringPrintf(
              R"([{"tabId": %d}, "Target.attachToTarget", )"
              R"({"targetId": "%s", "flatten": true}])",
              tab_id, GetGuestAgentHost()->GetId().c_str()),
          profile()),
      testing::HasSubstr("Not allowed"));
}

// Control test: verifies that a <webview> guest embedded in an ordinary
// extension page (not restricted WebUI) is enumerable and attachable by its
// own extension.
// Because the <webview> HTML tag is restricted to apps/allowlisted WebUIs,
// this attaches a WebViewGuest directly to a subframe via GuestViewManager.
IN_PROC_BROWSER_TEST_P(WebViewGuestDebuggerApiTest,
                       ControlExtensionWebViewGuestAllowed) {
  content::WebContents* wc = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(wc, extension()->GetResourceURL("test_file.html")));
  ASSERT_TRUE(content::ExecJs(
      wc, "document.body.appendChild(document.createElement('iframe'));"));
  auto* main = wc->GetPrimaryMainFrame();
  auto* frame = content::ChildFrameAt(main, 0);
  ASSERT_TRUE(frame);

  auto* guest_manager = GetGuestViewManager();
  base::test::TestFuture<std::unique_ptr<guest_view::GuestViewBase>> future;
  guest_manager->CreateGuestAndTransferOwnership(
      extensions::WebViewGuest::Type, main, nullptr, base::DictValue(),
      future.GetCallback());
  auto owned = future.Take();
  auto* raw = owned.get();
  base::DictValue params;
  params.Set("src", "about:blank");
  static_cast<guest_view::GuestViewManager*>(guest_manager)
      ->AttachGuest(main->GetProcess()->GetID(), frame->GetRoutingID(),
                    raw->guest_instance_id(), params);
  base::RunLoop loop;
  raw->AttachToOuterWebContentsFrame(std::move(owned), frame,
                                     frame->GetRoutingID(), false,
                                     loop.QuitClosure());
  loop.Run();
  ASSERT_TRUE(guest_manager->WaitUntilAttachedAndLoaded(raw));

  EXPECT_TRUE(HasTargetId(GetGuestAgentHost()->GetId()));

  auto attach = base::MakeRefCounted<DebuggerAttachFunction>();
  attach->set_extension(extension());
  EXPECT_TRUE(api_test_utils::RunFunction(
      attach.get(),
      base::StringPrintf(R"([{"targetId": "%s"}, "1.1"])",
                         GetGuestAgentHost()->GetId().c_str()),
      profile()));
}

// Tests that workers running in a WebUI-hosted guest partition cannot be
// attached to or enumerated.
IN_PROC_BROWSER_TEST_P(WebViewGuestDebuggerApiTest,
                       CannotAttachToOrEnumerateWebUiWebViewGuestWorkers) {
  if (GetParam()) {
    GTEST_SKIP() << "MPArch implementation skipped. https://crbug.com/40202416";
  }

  CreateWebUiGuestView();
  auto* guest_view = GetGuestViewManager()->GetLastGuestViewCreated();
  ASSERT_TRUE(guest_view);

  GURL guest_url =
      embedded_test_server()->GetURL("/workers/fetch_from_service_worker.html");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      guest_view->GetGuestMainFrame(), guest_url));

  ASSERT_EQ(true, content::EvalJs(guest_view->GetGuestMainFrame(), R"(
        new Promise(resolve => {
          const worker = new Worker('/workers/post_ready.js');
          worker.onmessage = () => resolve(true);
          window.dedicatedWorker = worker;
        });
      )"));

  ASSERT_EQ(true, content::EvalJs(guest_view->GetGuestMainFrame(), R"(
        new Promise(resolve => {
          const sw = new SharedWorker('/workers/shared_post_ready.js');
          sw.port.onmessage = () => resolve(true);
          sw.port.start();
          window.sharedWorker = sw;
        });
      )"));

  ASSERT_EQ("ready",
            content::EvalJs(guest_view->GetGuestMainFrame(), "setup();"));

  scoped_refptr<content::DevToolsAgentHost> dedicated_worker_host;
  scoped_refptr<content::DevToolsAgentHost> shared_worker_host;
  scoped_refptr<content::DevToolsAgentHost> service_worker_host;
  for (const auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetType() == content::DevToolsAgentHost::kTypeDedicatedWorker) {
      dedicated_worker_host = host;
    } else if (host->GetType() ==
                   content::DevToolsAgentHost::kTypeSharedWorker &&
               host->GetURL().spec().contains("shared_post_ready.js")) {
      shared_worker_host = host;
    } else if (host->GetType() ==
                   content::DevToolsAgentHost::kTypeServiceWorker &&
               host->GetURL().spec().contains("fetch_from_service_worker.js")) {
      service_worker_host = host;
    }
  }
  ASSERT_TRUE(dedicated_worker_host);
  ASSERT_TRUE(shared_worker_host);
  ASSERT_TRUE(service_worker_host);

  EXPECT_FALSE(HasTargetId(GetGuestAgentHost()->GetId()));
  EXPECT_FALSE(HasTargetId(dedicated_worker_host->GetId()));
  EXPECT_FALSE(HasTargetId(shared_worker_host->GetId()));
  EXPECT_FALSE(HasTargetId(service_worker_host->GetId()));

  for (const auto& worker_host :
       {dedicated_worker_host, shared_worker_host, service_worker_host}) {
    auto attach = base::MakeRefCounted<DebuggerAttachFunction>();
    attach->set_extension(extension());
    EXPECT_EQ(manifest_errors::kCannotAccessChromeUrl,
              api_test_utils::RunFunctionAndReturnError(
                  attach.get(),
                  base::StringPrintf(R"([{"targetId": "%s"}, "1.1"])",
                                     worker_host->GetId().c_str()),
                  profile()));
  }
}

// Control test: verifies that workers running in a <webview> guest embedded in
// an ordinary extension page (not restricted WebUI) are enumerable and
// attachable by its own extension.
IN_PROC_BROWSER_TEST_P(WebViewGuestDebuggerApiTest,
                       ControlExtensionWebViewGuestWorkersAllowed) {
  if (GetParam()) {
    GTEST_SKIP() << "MPArch implementation skipped. https://crbug.com/40202416";
  }

  content::WebContents* wc = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(wc, extension()->GetResourceURL("test_file.html")));
  ASSERT_TRUE(content::ExecJs(
      wc, "document.body.appendChild(document.createElement('iframe'));"));
  auto* main = wc->GetPrimaryMainFrame();
  auto* frame = content::ChildFrameAt(main, 0);
  ASSERT_TRUE(frame);

  // Because the <webview> HTML tag is restricted to platform apps and
  // allowlisted WebUIs (which the test extension lacks), attach a WebViewGuest
  // directly to an extension subframe via GuestViewManager.
  auto* guest_manager = GetGuestViewManager();
  base::test::TestFuture<std::unique_ptr<guest_view::GuestViewBase>> future;
  guest_manager->CreateGuestAndTransferOwnership(
      extensions::WebViewGuest::Type, main, nullptr, base::DictValue(),
      future.GetCallback());
  auto owned = future.Take();
  auto* raw = owned.get();
  base::DictValue params;
  params.Set("src", "about:blank");
  static_cast<guest_view::GuestViewManager*>(guest_manager)
      ->AttachGuest(main->GetProcess()->GetID(), frame->GetRoutingID(),
                    raw->guest_instance_id(), params);
  base::RunLoop loop;
  raw->AttachToOuterWebContentsFrame(std::move(owned), frame,
                                     frame->GetRoutingID(), false,
                                     loop.QuitClosure());
  loop.Run();
  ASSERT_TRUE(guest_manager->WaitUntilAttachedAndLoaded(raw));

  GURL guest_url =
      embedded_test_server()->GetURL("/workers/fetch_from_service_worker.html");
  ASSERT_TRUE(
      content::NavigateToURLFromRenderer(raw->GetGuestMainFrame(), guest_url));

  ASSERT_EQ(true, content::EvalJs(raw->GetGuestMainFrame(), R"(
        new Promise(resolve => {
          const worker = new Worker('/workers/post_ready.js');
          worker.onmessage = () => resolve(true);
          window.dedicatedWorker = worker;
        });
      )"));

  ASSERT_EQ(true, content::EvalJs(raw->GetGuestMainFrame(), R"(
        new Promise(resolve => {
          const sw = new SharedWorker('/workers/shared_post_ready.js');
          sw.port.onmessage = () => resolve(true);
          sw.port.start();
          window.sharedWorker = sw;
        });
      )"));

  ASSERT_EQ("ready", content::EvalJs(raw->GetGuestMainFrame(), "setup();"));

  scoped_refptr<content::DevToolsAgentHost> dedicated_worker_host;
  scoped_refptr<content::DevToolsAgentHost> shared_worker_host;
  scoped_refptr<content::DevToolsAgentHost> service_worker_host;
  for (const auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetType() == content::DevToolsAgentHost::kTypeDedicatedWorker) {
      dedicated_worker_host = host;
    } else if (host->GetType() ==
                   content::DevToolsAgentHost::kTypeSharedWorker &&
               host->GetURL().spec().contains("shared_post_ready.js")) {
      shared_worker_host = host;
    } else if (host->GetType() ==
                   content::DevToolsAgentHost::kTypeServiceWorker &&
               host->GetURL().spec().contains("fetch_from_service_worker.js")) {
      service_worker_host = host;
    }
  }
  ASSERT_TRUE(dedicated_worker_host);
  ASSERT_TRUE(shared_worker_host);
  ASSERT_TRUE(service_worker_host);

  for (const auto& worker_host :
       {dedicated_worker_host, shared_worker_host, service_worker_host}) {
    EXPECT_TRUE(HasTargetId(worker_host->GetId()));
    auto attach = base::MakeRefCounted<DebuggerAttachFunction>();
    attach->set_extension(extension());
    EXPECT_TRUE(api_test_utils::RunFunction(
        attach.get(),
        base::StringPrintf(R"([{"targetId": "%s"}, "1.1"])",
                           worker_host->GetId().c_str()),
        profile()));
  }
}

// Tests that a session already attached to a normal tab cannot auto-attach
// to or receive CDP events for WebUI guest workers.
IN_PROC_BROWSER_TEST_P(
    WebViewGuestDebuggerApiTest,
    AlreadyAttachedSessionCannotAutoAttachToWebUiWebViewGuestWorker) {
  if (GetParam()) {
    GTEST_SKIP() << "MPArch implementation skipped. https://crbug.com/40202416";
  }

  CreateWebUiGuestView();
  auto* guest_view = GetGuestViewManager()->GetLastGuestViewCreated();
  ASSERT_TRUE(guest_view);

  GURL guest_url =
      embedded_test_server()->GetURL("/workers/fetch_from_service_worker.html");
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      guest_view->GetGuestMainFrame(), guest_url));

  ASSERT_EQ(true, content::EvalJs(guest_view->GetGuestMainFrame(), R"(
        new Promise(resolve => {
          const worker = new Worker('/workers/post_ready.js');
          worker.onmessage = () => resolve(true);
          window.dedicatedWorker = worker;
        });
      )"));

  ASSERT_EQ(true, content::EvalJs(guest_view->GetGuestMainFrame(), R"(
        new Promise(resolve => {
          const sw = new SharedWorker('/workers/shared_post_ready.js');
          sw.port.onmessage = () => resolve(true);
          sw.port.start();
          window.sharedWorker = sw;
        });
      )"));

  ASSERT_EQ("ready",
            content::EvalJs(guest_view->GetGuestMainFrame(), "setup();"));

  scoped_refptr<content::DevToolsAgentHost> service_worker_host;
  scoped_refptr<content::DevToolsAgentHost> dedicated_worker_host;
  scoped_refptr<content::DevToolsAgentHost> shared_worker_host;
  for (const auto& host : content::DevToolsAgentHost::GetOrCreateAll()) {
    if (host->GetType() == content::DevToolsAgentHost::kTypeServiceWorker &&
        host->GetURL().spec().contains("fetch_from_service_worker.js")) {
      service_worker_host = host;
    } else if (host->GetType() ==
               content::DevToolsAgentHost::kTypeDedicatedWorker) {
      dedicated_worker_host = host;
    } else if (host->GetType() ==
                   content::DevToolsAgentHost::kTypeSharedWorker &&
               host->GetURL().spec().contains("shared_post_ready.js")) {
      shared_worker_host = host;
    }
  }
  ASSERT_TRUE(service_worker_host);
  ASSERT_TRUE(dedicated_worker_host);
  ASSERT_TRUE(shared_worker_host);

  content::RenderFrameHost* second_tab_rfh =
      NavigateToURLInNewTab(GURL("about:blank"));
  ASSERT_TRUE(second_tab_rfh);
  int second_tab_id =
      sessions::SessionTabHelper::IdForTab(
          content::WebContents::FromRenderFrameHost(second_tab_rfh))
          .id();

  auto attach_second = base::MakeRefCounted<DebuggerAttachFunction>();
  attach_second->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      attach_second.get(),
      base::StringPrintf(R"([{"tabId": %d}, "1.3"])", second_tab_id),
      profile()));

  for (const auto& worker_host :
       {dedicated_worker_host, shared_worker_host, service_worker_host}) {
    auto send_cmd_attach = base::MakeRefCounted<DebuggerSendCommandFunction>();
    send_cmd_attach->set_extension(extension());
    std::string attach_error = api_test_utils::RunFunctionAndReturnError(
        send_cmd_attach.get(),
        base::StringPrintf(
            R"([{"tabId": %d}, "Target.attachToTarget", )"
            R"({"targetId": "%s", "flatten": true}])",
            second_tab_id, worker_host->GetId().c_str()),
        profile());
    EXPECT_THAT(attach_error, testing::HasSubstr("Not allowed"));
  }

  extensions::TestEventRouterObserver event_observer(
      extensions::EventRouter::Get(profile()));

  ASSERT_TRUE(NavigateToURL(
      content::WebContents::FromRenderFrameHost(second_tab_rfh), guest_url));

  auto send_cmd_auto_attach =
      base::MakeRefCounted<DebuggerSendCommandFunction>();
  send_cmd_auto_attach->set_extension(extension());
  ASSERT_TRUE(api_test_utils::RunFunction(
      send_cmd_auto_attach.get(),
      base::StringPrintf(
          R"([{"tabId": %d}, "Target.setAutoAttach", )"
          R"({"autoAttach": true, "waitForDebuggerOnStart": false, )"
          R"("flatten": true}])",
          second_tab_id),
      profile()));

  for (const auto& event : event_observer.all_events()) {
    std::string event_str = base::WriteJson(event->args()).value_or("");
    EXPECT_THAT(event_str,
                testing::Not(testing::HasSubstr(service_worker_host->GetId())));
    EXPECT_THAT(event_str,
                testing::Not(testing::HasSubstr(GetGuestAgentHost()->GetId())));
    EXPECT_THAT(
        event_str,
        testing::Not(testing::HasSubstr(dedicated_worker_host->GetId())));
    EXPECT_THAT(event_str,
                testing::Not(testing::HasSubstr(shared_worker_host->GetId())));
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service_worker_host->IsAttached());

  auto detach_second = base::MakeRefCounted<DebuggerDetachFunction>();
  detach_second->set_extension(extension());
  EXPECT_TRUE(api_test_utils::RunFunction(
      detach_second.get(),
      base::StringPrintf(R"([{"tabId": %d}])", second_tab_id), profile()));
}

INSTANTIATE_TEST_SUITE_P(All, WebViewGuestDebuggerApiTest, testing::Bool());

#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)
}  // namespace extensions
