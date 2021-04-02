// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace extensions {

namespace {

const char kTestCustomArg[] = "customArg";
const char kTestDataDirectory[] = "testDataDirectory";
const char kTestWebSocketPort[] = "testWebSocketPort";
const char kFtpServerPort[] = "ftpServer.port";
const char kEmbeddedTestServerPort[] = "testServer.port";

}  // namespace

ExtensionApiTest::ExtensionApiTest() {
  net::test_server::RegisterDefaultHandlers(embedded_test_server());
}

ExtensionApiTest::~ExtensionApiTest() = default;

void ExtensionApiTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  DCHECK(!test_config_.get()) << "Previous test did not clear config state.";
  test_config_ = std::make_unique<base::DictionaryValue>();
  test_config_->SetString(kTestDataDirectory,
                          net::FilePathToFileURL(test_data_dir_).spec());

  if (embedded_test_server()->Started()) {
    // InitializeEmbeddedTestServer was called before |test_config_| was set.
    // Set the missing port key.
    test_config_->SetInteger(kEmbeddedTestServerPort,
                             embedded_test_server()->port());
  }

  TestGetConfigFunction::set_test_config_state(test_config_.get());
}

void ExtensionApiTest::TearDownOnMainThread() {
  ExtensionBrowserTest::TearDownOnMainThread();
  TestGetConfigFunction::set_test_config_state(NULL);
  test_config_.reset(NULL);
}

bool ExtensionApiTest::RunExtensionTest(const RunOptions& run_options) {
  return RunExtensionTest(run_options, {});
}

bool ExtensionApiTest::RunExtensionTest(const char* extension_name) {
  return RunExtensionTest({.name = extension_name}, {});
}

bool ExtensionApiTest::RunExtensionTest(const RunOptions& run_options,
                                        const LoadOptions& load_options) {
  // Do some sanity checks for options that are mutually exclusive or
  // only valid with other options.
  CHECK(run_options.name || run_options.page_url)
      << "Must specify either 'name' or 'page_url'";
  CHECK(!(run_options.extension_url && run_options.page_url))
      << "'extension_url' and 'page_url' are mutually exclusive.";
  CHECK(!run_options.open_in_incognito || run_options.page_url)
      << "'open_in_incognito' is only allowed if specifiying 'page_url'";
  CHECK(!(run_options.launch_as_platform_app && run_options.page_url))
      << "'launch_as_platform_app' and 'page_url' are mutually exclusive.";

  if (run_options.custom_arg)
    SetCustomArg(run_options.custom_arg);

  ResultCatcher catcher;

  const Extension* extension = nullptr;
  if (run_options.name) {
    const base::FilePath& root_path = run_options.use_extensions_root_dir
                                          ? shared_test_data_dir_
                                          : test_data_dir_;
    base::FilePath extension_path = root_path.AppendASCII(run_options.name);
    // TODO(https://crbug.com/1171429): Move load_as_component into LoadOptions
    // and unify LoadExtension and LoadExtensionAsComponent.
    // As it stands today, all LoadOptions will be ignored when loading the
    // extension as a component extension.
    if (run_options.load_as_component) {
      extension = LoadExtensionAsComponent(extension_path);
    } else {
      extension = LoadExtension(extension_path, load_options);
    }
    if (!extension) {
      message_ = "Failed to load extension.";
      return false;
    }
  }

  // If there is a page_url to load, navigate it.
  // TODO(https://crbug.com/1171429): Separate page_url into page_url and
  // extension_url.
  if (run_options.page_url) {
    GURL url(run_options.page_url);

    // Note: We use is_valid() here in the expectation that the provided url
    // may lack a scheme & host and thus be a relative url within the loaded
    // extension.
    if (!url.is_valid()) {
      DCHECK(run_options.name) << "Relative page_url given with no name";

      url = extension->GetResourceURL(run_options.page_url);
    }

    if (run_options.open_in_incognito)
      OpenURLOffTheRecord(browser()->profile(), url);
    else
      ui_test_utils::NavigateToURL(browser(), url);
  } else if (run_options.launch_as_platform_app) {
    apps::AppLaunchParams params(
        extension->id(), LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_WINDOW, AppLaunchSource::kSourceTest);
    params.command_line = *base::CommandLine::ForCurrentProcess();
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(std::move(params));
  }

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }

  return true;
}

bool ExtensionApiTest::RunExtensionSubtest(const std::string& extension_name,
                                           const std::string& page_url) {
  DCHECK(!page_url.empty()) << "Argument page_url is required.";
  return RunExtensionTestImpl(extension_name, page_url, nullptr, kFlagNone,
                              kFlagNone);
}

bool ExtensionApiTest::RunExtensionTestImpl(const std::string& extension_name,
                                            const std::string& page_url,
                                            const char* custom_arg,
                                            int browser_test_flags,
                                            int api_test_flags) {
  static_assert(static_cast<int>(ExtensionBrowserTest::kFlagNone) ==
                    static_cast<int>(ExtensionApiTest::kFlagNone),
                "ExtensionApiTest::kFlagNone has an incorrect value");
  // The value of "api_test_flags" should not have any of the flags from the
  // ExtensionBrowserTest::Flags range. "kFlagNextValue - 1" is all of the
  // bits from that range.
  CHECK_EQ(0, api_test_flags & (kFlagNextValue - 1));
  bool load_as_component =
      api_test_flags & ExtensionApiTest::kFlagLoadAsComponent;
  bool launch_platform_app =
      api_test_flags & ExtensionApiTest::kFlagLaunchPlatformApp;
  bool use_incognito = api_test_flags & kFlagUseIncognito;
  bool use_root_extensions_dir =
      api_test_flags & ExtensionApiTest::kFlagUseRootExtensionsDir;

  if (custom_arg && custom_arg[0])
    SetCustomArg(custom_arg);

  ResultCatcher catcher;
  DCHECK(!extension_name.empty() || !page_url.empty()) <<
      "extension_name and page_url cannot both be empty";

  const Extension* extension = NULL;
  if (!extension_name.empty()) {
    const base::FilePath& root_path =
        use_root_extensions_dir ? shared_test_data_dir_ : test_data_dir_;
    base::FilePath extension_path = root_path.AppendASCII(extension_name);
    if (load_as_component) {
      extension = LoadExtensionAsComponent(extension_path);
    } else {
      // TODO(crbug.com/1171429): This call needs to be removed when this
      // bug is addressed for the ExtensionApiTest "RunExtensionTest"
      // overloads.
      extension = LoadExtensionWithInstallParam(
          extension_path, browser_test_flags, std::string());
    }
    if (!extension) {
      message_ = "Failed to load extension.";
      return false;
    }
  }

  // If there is a page_url to load, navigate it.
  if (!page_url.empty()) {
    GURL url = GURL(page_url);

    // Note: We use is_valid() here in the expectation that the provided url
    // may lack a scheme & host and thus be a relative url within the loaded
    // extension.
    if (!url.is_valid()) {
      DCHECK(!extension_name.empty()) <<
          "Relative page_url given with no extension_name";

      url = extension->GetResourceURL(page_url);
    }

    if (use_incognito)
      OpenURLOffTheRecord(browser()->profile(), url);
    else
      ui_test_utils::NavigateToURL(browser(), url);
  } else if (launch_platform_app) {
    apps::AppLaunchParams params(
        extension->id(), LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_WINDOW, AppLaunchSource::kSourceTest);
    params.command_line = *base::CommandLine::ForCurrentProcess();
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(std::move(params));
  }

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }

  return true;
}

// Test that exactly one extension is loaded, and return it.
const Extension* ExtensionApiTest::GetSingleLoadedExtension() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());

  const Extension* result = NULL;
  for (const scoped_refptr<const Extension>& extension :
       registry->enabled_extensions()) {
    // Ignore any component extensions. They are automatically loaded into all
    // profiles and aren't the extension we're looking for here.
    if (extension->location() == mojom::ManifestLocation::kComponent)
      continue;

    if (result != NULL) {
      // TODO(yoz): this is misleading; it counts component extensions.
      message_ = base::StringPrintf(
          "Expected only one extension to be present.  Found %u.",
          static_cast<unsigned>(registry->enabled_extensions().size()));
      return NULL;
    }

    result = extension.get();
  }

  if (!result) {
    message_ = "extension pointer is NULL.";
    return NULL;
  }
  return result;
}

bool ExtensionApiTest::StartEmbeddedTestServer() {
  if (!InitializeEmbeddedTestServer())
    return false;

  EmbeddedTestServerAcceptConnections();
  return true;
}

bool ExtensionApiTest::InitializeEmbeddedTestServer() {
  if (!embedded_test_server()->InitializeAndListen())
    return false;

  // Build a dictionary of values that tests can use to build URLs that
  // access the test server and local file system.  Tests can see these values
  // using the extension API function chrome.test.getConfig().
  if (test_config_) {
    test_config_->SetInteger(kEmbeddedTestServerPort,
                             embedded_test_server()->port());
  }
  // else SetUpOnMainThread has not been called yet. Possibly because the
  // caller needs a valid port in an overridden SetUpCommandLine method.

  return true;
}

void ExtensionApiTest::EmbeddedTestServerAcceptConnections() {
  embedded_test_server()->StartAcceptingConnections();
}

bool ExtensionApiTest::StartWebSocketServer(
    const base::FilePath& root_directory,
    bool enable_basic_auth) {
  websocket_server_ = std::make_unique<net::SpawnedTestServer>(
      net::SpawnedTestServer::TYPE_WS, root_directory);
  websocket_server_->set_websocket_basic_auth(enable_basic_auth);

  if (!websocket_server_->Start())
    return false;

  test_config_->SetInteger(kTestWebSocketPort,
                           websocket_server_->host_port_pair().port());

  return true;
}

bool ExtensionApiTest::StartFTPServer(const base::FilePath& root_directory) {
  ftp_server_ = std::make_unique<net::SpawnedTestServer>(
      net::SpawnedTestServer::TYPE_FTP, root_directory);

  if (!ftp_server_->Start())
    return false;

  test_config_->SetInteger(kFtpServerPort,
                           ftp_server_->host_port_pair().port());

  return true;
}

void ExtensionApiTest::SetCustomArg(base::StringPiece custom_arg) {
  test_config_->SetKey(kTestCustomArg, base::Value(custom_arg));
}

void ExtensionApiTest::SetUpCommandLine(base::CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);

  test_data_dir_ = test_data_dir_.AppendASCII("api_test");

  RegisterPathProvider();
  base::PathService::Get(DIR_TEST_DATA, &shared_test_data_dir_);
  shared_test_data_dir_ = shared_test_data_dir_.AppendASCII("api_test");

  // Backgrounded renderer processes run at a lower priority, causing the
  // tests to take more time to complete. Disable backgrounding so that the
  // tests don't time out.
  command_line->AppendSwitch(::switches::kDisableRendererBackgrounding);
}

}  // namespace extensions
