// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_run_loop_timeout.h"
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
#include "components/services/app_service/public/cpp/app_launch_util.h"
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
const char kEmbeddedTestServerPort[] = "testServer.port";

}  // namespace

ExtensionApiTest::ExtensionApiTest(ContextType context_type)
    : ExtensionBrowserTest(context_type) {
  net::test_server::RegisterDefaultHandlers(embedded_test_server());
}

ExtensionApiTest::~ExtensionApiTest() = default;

void ExtensionApiTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  DCHECK(!test_config_.get()) << "Previous test did not clear config state.";
  test_config_ = std::make_unique<base::Value::Dict>();
  test_config_->Set(kTestDataDirectory,
                    net::FilePathToFileURL(test_data_dir_).spec());

  if (embedded_test_server()->Started()) {
    // InitializeEmbeddedTestServer was called before |test_config_| was set.
    // Set the missing port key.
    test_config_->SetByDottedPath(kEmbeddedTestServerPort,
                                  embedded_test_server()->port());
  }

  TestGetConfigFunction::set_test_config_state(test_config_.get());
}

void ExtensionApiTest::TearDownOnMainThread() {
  ExtensionBrowserTest::TearDownOnMainThread();
  TestGetConfigFunction::set_test_config_state(nullptr);
  test_config_.reset();
}

bool ExtensionApiTest::RunExtensionTest(const char* extension_name) {
  return RunExtensionTest(extension_name, {}, {});
}

bool ExtensionApiTest::RunExtensionTest(const char* extension_name,
                                        const RunOptions& run_options) {
  return RunExtensionTest(extension_name, run_options, {});
}

bool ExtensionApiTest::RunExtensionTest(const char* extension_name,
                                        const RunOptions& run_options,
                                        const LoadOptions& load_options) {
  const base::FilePath& root_path = run_options.use_extensions_root_dir
                                        ? shared_test_data_dir_
                                        : test_data_dir_;
  base::FilePath extension_path = root_path.AppendASCII(extension_name);
  return RunExtensionTest(extension_path, run_options, load_options);
}

bool ExtensionApiTest::RunExtensionTest(const base::FilePath& extension_path,
                                        const RunOptions& run_options,
                                        const LoadOptions& load_options) {
  // Do some sanity checks for options that are mutually exclusive or
  // only valid with other options.
  CHECK(!(run_options.extension_url && run_options.page_url))
      << "'extension_url' and 'page_url' are mutually exclusive.";
  CHECK(!run_options.open_in_incognito || run_options.page_url ||
        run_options.extension_url)
      << "'open_in_incognito' is only allowed if specifiying 'page_url'";
  CHECK(!(run_options.launch_as_platform_app && run_options.page_url))
      << "'launch_as_platform_app' and 'page_url' are mutually exclusive.";

  if (run_options.custom_arg)
    SetCustomArg(run_options.custom_arg);

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(extension_path, load_options);
  if (!extension) {
    message_ = "Failed to load extension.";
    return false;
  }

  GURL url_to_open;
  if (run_options.page_url) {
    url_to_open = GURL(run_options.page_url);
    DCHECK(url_to_open.has_scheme() && url_to_open.has_host());
    // Note: We use is_valid() here in the expectation that the provided url
    // may lack a scheme & host and thus be a relative url within the loaded
    // extension.
    // TODO(crbug.com/40210201): Update callers passing relative paths
    // for page URLs to instead use extension_url.
    if (!url_to_open.is_valid())
      url_to_open = extension->GetResourceURL(run_options.page_url);
  } else if (run_options.extension_url) {
    DCHECK(!url_to_open.has_scheme() && !url_to_open.has_host());
    url_to_open = extension->GetResourceURL(run_options.extension_url);
  }

  // If there is a page_url to load, navigate it.
  if (!url_to_open.is_empty()) {
    OpenURL(url_to_open, run_options.open_in_incognito);
  } else if (run_options.launch_as_platform_app) {
    apps::AppLaunchParams params(
        extension->id(), apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
    params.command_line = *base::CommandLine::ForCurrentProcess();
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(std::move(params));
  }

  {
    base::test::ScopedRunLoopTimeout timeout(
        FROM_HERE, std::nullopt,
        base::BindRepeating(
            [](const base::FilePath& extension_path) {
              return "GetNextResult timeout while RunExtensionTest: " +
                     extension_path.MaybeAsASCII();
            },
            extension_path));
    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }
  }

  return true;
}

void ExtensionApiTest::OpenURL(const GURL& url, bool open_in_incognito) {
  if (open_in_incognito) {
    OpenURLOffTheRecord(browser()->profile(), url);
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }
}

bool ExtensionApiTest::OpenTestURL(const GURL& url, bool open_in_incognito) {
  DCHECK(url.is_valid());

  ResultCatcher catcher;

  OpenURL(url, open_in_incognito);

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }

  return true;
}

// Test that exactly one extension is loaded, and return it.
const Extension* ExtensionApiTest::GetSingleLoadedExtension() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());

  const Extension* result = nullptr;
  for (const scoped_refptr<const Extension>& extension :
       registry->enabled_extensions()) {
    // Ignore any component extensions. They are automatically loaded into all
    // profiles and aren't the extension we're looking for here.
    if (extension->location() == mojom::ManifestLocation::kComponent)
      continue;

    if (result != nullptr) {
      // TODO(yoz): this is misleading; it counts component extensions.
      message_ = base::StringPrintf(
          "Expected only one extension to be present.  Found %u.",
          static_cast<unsigned>(registry->enabled_extensions().size()));
      return nullptr;
    }

    result = extension.get();
  }

  if (!result) {
    message_ = "extension pointer is NULL.";
    return nullptr;
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
    test_config_->SetByDottedPath(kEmbeddedTestServerPort,
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

  test_config_->Set(kTestWebSocketPort,
                    websocket_server_->host_port_pair().port());

  return true;
}

void ExtensionApiTest::SetCustomArg(std::string_view custom_arg) {
  test_config_->Set(kTestCustomArg, base::Value(custom_arg));
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

void ExtensionApiTest::UseHttpsTestServer() {
  https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_.get()->AddDefaultHandlers(GetChromeTestDataDir());
  https_test_server_.get()->SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
}

}  // namespace extensions
