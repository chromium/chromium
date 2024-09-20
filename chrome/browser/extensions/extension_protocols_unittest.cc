// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_protocols.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/power_monitor_test.h"
#include "base/test/test_file_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"

using extensions::ExtensionRegistry;
using network::mojom::URLLoader;
using testing::_;
using testing::StrictMock;

namespace extensions {
namespace {

constexpr char kValidTrialToken1[] = "valid_token_1";
constexpr char kValidTrialToken2[] = "valid_token_2";
constexpr char kTrialTokensHeaderValue[] = "valid_token_1, valid_token_2";

base::FilePath GetTestPath(const std::string& name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  return path.AppendASCII("extensions").AppendASCII(name);
}

base::FilePath GetContentVerifierTestPath() {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(DIR_TEST_DATA, &path));
  return path.AppendASCII("content_hash_fetcher")
      .AppendASCII("different_sized_files");
}

scoped_refptr<const Extension> CreateTestExtension(const std::string& name,
                                                   bool incognito_split_mode,
                                                   int manifest_version) {
  return ExtensionBuilder(name)
      .SetManifestVersion(manifest_version)
      .SetManifestKey("incognito", incognito_split_mode ? "split" : "spanning")
      .SetPath(GetTestPath("response_headers"))
      .SetLocation(mojom::ManifestLocation::kInternal)
      .Build();
}

scoped_refptr<const Extension> CreateWebStoreExtension(int manifest_version) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_RESOURCES, &path));
  path = path.AppendASCII("web_store");

  return ExtensionBuilder("WebStore")
      .SetManifestVersion(manifest_version)
      .SetManifestKey("icons",
                      base::Value::Dict().Set("16", "webstore_icon_16.png"))
      .SetManifestKey(
          "web_accessible_resources",
          manifest_version == 3
              ? base::Value::List().Append(
                    base::Value::Dict()
                        .Set("resources",
                             base::Value::List().Append("webstore_icon_16.png"))
                        .Set("matches", base::Value::List().Append("*://*/*")))
              : base::Value::List().Append("webstore_icon_16.png"))
      .SetPath(path)
      .SetLocation(mojom::ManifestLocation::kComponent)
      .Build();
}

scoped_refptr<const Extension> CreateTestResponseHeaderExtension(
    int manifest_version) {
  if (manifest_version == 3) {
    return ExtensionBuilder("An extension with web-accessible resources")
        .SetManifestVersion(3)
        .SetManifestKey(
            "web_accessible_resources",
            base::Value::List().Append(
                base::Value::Dict()
                    .Set("resources", base::Value::List().Append("test.dat"))
                    .Set("matches", base::Value::List().Append("*://*/*"))))
        .SetManifestKey("background", base::Value::Dict().Set("service_worker",
                                                              "background.js"))
        .SetManifestKey("trial_tokens", base::Value::List()
                                            .Append(kValidTrialToken1)
                                            .Append(kValidTrialToken2))
        .SetPath(GetTestPath("response_headers"))
        .Build();
  }
  return ExtensionBuilder("An extension with web-accessible resources")
      .SetManifestVersion(manifest_version)
      .SetManifestKey("web_accessible_resources",
                      base::Value::List().Append("test.dat"))
      .SetManifestKey(
          "background",
          base::Value::Dict().Set("scripts",
                                  base::Value::List().Append("background.js")))
      .SetPath(GetTestPath("response_headers"))
      .Build();
}

scoped_refptr<const Extension> CreateTestModuleResponseHeaderExtension(
    int manifest_version) {
  return ExtensionBuilder("A module extension")
      .SetManifestVersion(manifest_version)
      .SetManifestKey("export", base::Value::Dict())
      .SetPath(GetTestPath("response_headers"))
      .Build();
}

scoped_refptr<const Extension> CreateTestModuleImporterResponseHeaderExtension(
    int manifest_version,
    const std::string& module_extension_id) {
  if (manifest_version == 3) {
    return ExtensionBuilder("A module importer extension")
        .SetManifestVersion(3)
        .SetManifestKey("import",
                        base::Value::List().Append(
                            base::Value::Dict().Set("id", module_extension_id)))
        .SetManifestKey("trial_tokens", base::Value::List()
                                            .Append(kValidTrialToken1)
                                            .Append(kValidTrialToken2))
        .SetPath(GetTestPath("response_headers"))
        .Build();
  }
  return ExtensionBuilder("A module importer extension")
      .SetManifestVersion(manifest_version)
      .SetManifestKey("import",
                      base::Value::List().Append(
                          base::Value::Dict().Set("id", module_extension_id)))
      .SetPath(GetTestPath("response_headers"))
      .Build();
}

// Helper function to create a |ResourceRequest| for testing purposes.
network::ResourceRequest CreateResourceRequest(
    const std::string& method,
    network::mojom::RequestDestination destination,
    const GURL& url) {
  network::ResourceRequest request;
  request.method = method;
  request.url = url;
  request.site_for_cookies =
      net::SiteForCookies::FromUrl(url);  // bypass third-party cookie blocking.
  request.request_initiator =
      url::Origin::Create(url);  // ensure initiator set.
  request.referrer_policy = blink::ReferrerUtils::GetDefaultNetReferrerPolicy();
  request.destination = destination;
  request.is_outermost_main_frame =
      destination == network::mojom::RequestDestination::kDocument;
  return request;
}

// The result of either a URLRequest of a URLLoader response (but not both)
// depending on the on test type.
class GetResult {
 public:
  GetResult(network::mojom::URLResponseHeadPtr response, int result)
      : response_(std::move(response)), result_(result) {}
  GetResult(GetResult&& other) : result_(other.result_) {}

  GetResult(const GetResult&) = delete;
  GetResult& operator=(const GetResult&) = delete;

  ~GetResult() = default;

  std::string GetResponseHeaderByName(const std::string& name) const {
    std::string value;
    if (response_ && response_->headers)
      response_->headers->GetNormalizedHeader(name, &value);
    return value;
  }

  bool HasContentLengthHeader() {
    std::string content_length =
        GetResponseHeaderByName(net::HttpRequestHeaders::kContentLength);

    int length_value = 0;
    return !content_length.empty() &&
           base::StringToInt(content_length, &length_value) && length_value > 0;
  }

  bool HeaderIsPresent(const std::string& name) {
    return !GetResponseHeaderByName(name).empty();
  }

  int result() const { return result_; }

 private:
  network::mojom::URLResponseHeadPtr response_;
  int result_;
};

}  // namespace

// This test lives in src/chrome instead of src/extensions because it tests
// functionality delegated back to Chrome via ChromeExtensionsBrowserClient.
// See chrome/browser/extensions/chrome_url_request_util.cc.
class ExtensionProtocolsTestBase : public testing::Test,
                                   public testing::WithParamInterface<int> {
 public:
  explicit ExtensionProtocolsTestBase(bool force_incognito)
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        rvh_test_enabler_(new content::RenderViewHostTestEnabler()),
        force_incognito_(force_incognito) {}

  void SetUp() override {
    testing::Test::SetUp();
    testing_profile_ = TestingProfile::Builder().Build();
    contents_ = CreateTestWebContents();

    // Set up content verification.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        switches::kExtensionContentVerification,
        switches::kExtensionContentVerificationEnforce);
    content_verifier_ = new ContentVerifier(
        browser_context(),
        std::make_unique<ChromeContentVerifierDelegate>(browser_context()));
    content_verifier_->Start();
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(browser_context()))
        ->set_content_verifier(content_verifier_.get());
    loader_factory_.Bind(
        CreateExtensionNavigationURLLoaderFactory(browser_context(), false));
  }

  void TearDown() override {
    loader_factory_.reset();
    content_verifier_->Shutdown();
    // Shut down the PowerMonitor if initialized.
    base::PowerMonitor::GetInstance()->ShutdownForTesting();
  }

  GetResult RequestOrLoad(const GURL& url,
                          network::mojom::RequestDestination destination) {
    return LoadURL(url, destination);
  }

  void AddExtension(const scoped_refptr<const Extension>& extension,
                    bool incognito_enabled,
                    bool notifications_disabled) {
    EXPECT_TRUE(extension_registry()->AddEnabled(extension));
    extension_registry()->TriggerOnLoaded(extension.get());
    ExtensionPrefs::Get(browser_context())
        ->SetIsIncognitoEnabled(extension->id(), incognito_enabled);
  }

  void RemoveExtension(const scoped_refptr<const Extension>& extension,
                       const UnloadedExtensionReason reason) {
    EXPECT_TRUE(extension_registry()->RemoveEnabled(extension->id()));
    extension_registry()->TriggerOnUnloaded(extension.get(), reason);
    if (reason == UnloadedExtensionReason::DISABLE)
      EXPECT_TRUE(extension_registry()->AddDisabled(extension));
  }

  // Helper method to create a URL request/loader, call RequestOrLoad on it, and
  // return the result. If |extension| hasn't already been added to
  // extension_registry(), this will add it.
  GetResult DoRequestOrLoad(const scoped_refptr<Extension> extension,
                            const std::string& relative_path) {
    if (!extension_registry()->enabled_extensions().Contains(extension->id())) {
      AddExtension(extension.get(),
                   /*incognito_enabled=*/false,
                   /*notifications_disabled=*/false);
    }
    return RequestOrLoad(extension->GetResourceURL(relative_path),
                         network::mojom::RequestDestination::kDocument);
  }

  ExtensionRegistry* extension_registry() {
    return ExtensionRegistry::Get(browser_context());
  }

  content::BrowserContext* browser_context() {
    return force_incognito_ ? testing_profile_->GetPrimaryOTRProfile(
                                  /*create_if_needed=*/true)
                            : testing_profile_.get();
  }

  void EnableSimulationOfSystemSuspendForRequests() {
    power_monitor_source_.emplace();
  }

 protected:
  scoped_refptr<ContentVerifier> content_verifier_;

 private:
  GetResult LoadURL(const GURL& url,
                    network::mojom::RequestDestination destination) {
    constexpr int32_t kRequestId = 28;

    mojo::PendingRemote<network::mojom::URLLoader> loader;
    network::TestURLLoaderClient client;
    loader_factory_->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), kRequestId,
        network::mojom::kURLLoadOptionNone,
        CreateResourceRequest("GET", destination, url), client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    // If `power_monitor_source_` is set, simulates power suspend and resume
    // notifications. These notifications are posted tasks that will be executed
    // by `client.RunUntilComplete()`.
    if (power_monitor_source_) {
      power_monitor_source_->Suspend();
      power_monitor_source_->Resume();
    }

    client.RunUntilComplete();
    return GetResult(client.response_head().Clone(),
                     client.completion_status().error_code);
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    auto site_instance = content::SiteInstance::Create(browser_context());
    return content::WebContentsTester::CreateTestWebContents(
        browser_context(), std::move(site_instance));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<content::WebContents> contents_;
  const bool force_incognito_;

  std::optional<base::test::ScopedPowerMonitorTestSource> power_monitor_source_;
};

class ExtensionProtocolsTest : public ExtensionProtocolsTestBase {
 public:
  ExtensionProtocolsTest()
      : ExtensionProtocolsTestBase(false /*force_incognito*/) {}
};

class ExtensionProtocolsIncognitoTest : public ExtensionProtocolsTestBase {
 public:
  ExtensionProtocolsIncognitoTest()
      : ExtensionProtocolsTestBase(true /*force_incognito*/) {}
};

// A specialization that will only run on MV3 extensions.
using ExtensionProtocolsMV3Test = ExtensionProtocolsTest;

INSTANTIATE_TEST_SUITE_P(MV2, ExtensionProtocolsTest, ::testing::Values(2));
INSTANTIATE_TEST_SUITE_P(MV3, ExtensionProtocolsTest, ::testing::Values(3));
INSTANTIATE_TEST_SUITE_P(MV2,
                         ExtensionProtocolsIncognitoTest,
                         ::testing::Values(2));
INSTANTIATE_TEST_SUITE_P(MV3,
                         ExtensionProtocolsIncognitoTest,
                         ::testing::Values(3));
INSTANTIATE_TEST_SUITE_P(MV3, ExtensionProtocolsMV3Test, ::testing::Values(3));

// Tests that making a chrome-extension request in an incognito context is
// only allowed under the right circumstances (if the extension is allowed
// in incognito, and it's either a non-main-frame request or a split-mode
// extension).
TEST_P(ExtensionProtocolsIncognitoTest, IncognitoRequest) {
  struct TestCase {
    // Inputs.
    std::string name;
    bool incognito_split_mode;
    bool incognito_enabled;

    // Expected result.
    bool should_allow_main_frame_load;
  } test_cases[] = {
      {"spanning disabled", false, false, false},
      {"split disabled", true, false, false},
      {"spanning enabled", false, true, false},
      {"split enabled", true, true, true},
  };

  for (const auto& test_case : test_cases) {
    scoped_refptr<const Extension> extension = CreateTestExtension(
        test_case.name, test_case.incognito_split_mode, GetParam());
    AddExtension(extension, test_case.incognito_enabled, false);

    // First test a main frame request.
    // It doesn't matter that the resource doesn't exist. If the resource
    // is blocked, we should see BLOCKED_BY_CLIENT. Otherwise, the request
    // should just fail because the file doesn't exist.
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("404.html"),
                      network::mojom::RequestDestination::kDocument);

    if (test_case.should_allow_main_frame_load) {
      EXPECT_EQ(net::ERR_FILE_NOT_FOUND, get_result.result()) << test_case.name;
    } else {
      EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result())
          << test_case.name;
    }
    // Subframe navigation requests are blocked in ExtensionNavigationThrottle
    // which isn't added in this unit test. This is tested in an integration
    // test in ExtensionResourceRequestPolicyTest.IframeNavigateToInaccessible.
    RemoveExtension(extension, UnloadedExtensionReason::UNINSTALL);
  }
}

// Tests getting a resource for a component extension works correctly, both when
// the extension is enabled and when it is disabled.
TEST_P(ExtensionProtocolsTest, ComponentResourceRequest) {
  scoped_refptr<const Extension> extension =
      CreateWebStoreExtension(GetParam());
  AddExtension(extension, false, false);

  // First test it with the extension enabled.
  {
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("webstore_icon_16.png"),
                      network::mojom::RequestDestination::kVideo);
    EXPECT_EQ(net::OK, get_result.result());
    EXPECT_TRUE(get_result.HasContentLengthHeader());
    EXPECT_EQ("image/png", get_result.GetResponseHeaderByName(
                               net::HttpRequestHeaders::kContentType));
    // TODO(crbug.com/333078381): remove "Content-Security-Policy" header from
    // images.
    EXPECT_TRUE(get_result.HeaderIsPresent("Content-Security-Policy"));
  }

  // And then test it with the extension disabled.
  RemoveExtension(extension, UnloadedExtensionReason::DISABLE);
  {
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("webstore_icon_16.png"),
                      network::mojom::RequestDestination::kVideo);
    EXPECT_EQ(net::OK, get_result.result());
    EXPECT_TRUE(get_result.HasContentLengthHeader());
    EXPECT_EQ("image/png", get_result.GetResponseHeaderByName(
                               net::HttpRequestHeaders::kContentType));
  }
}

// Tests that a URL request for resource from an extension returns a few
// expected response headers.
TEST_P(ExtensionProtocolsTest, ResourceRequestResponseHeaders) {
  scoped_refptr<const Extension> extension =
      CreateTestResponseHeaderExtension(GetParam());
  AddExtension(extension, false, false);

  {
    auto get_result = RequestOrLoad(extension->GetResourceURL("test.dat"),
                                    network::mojom::RequestDestination::kVideo);
    EXPECT_EQ(net::OK, get_result.result());

    // Check that cache-related headers are set.
    std::string etag = get_result.GetResponseHeaderByName("ETag");
    EXPECT_TRUE(base::StartsWith(etag, "\"", base::CompareCase::SENSITIVE));
    EXPECT_TRUE(base::EndsWith(etag, "\"", base::CompareCase::SENSITIVE));

    EXPECT_EQ("no-cache", get_result.GetResponseHeaderByName("Cache-Control"));

    // We set test.dat as web-accessible, so it should have CORS headers.
    EXPECT_EQ(
        "*", get_result.GetResponseHeaderByName("Access-Control-Allow-Origin"));
    EXPECT_EQ("cross-origin", get_result.GetResponseHeaderByName(
                                  "Cross-Origin-Resource-Policy"));

    // Only background service worker script should be allowed to load as a
    // service worker.
    EXPECT_FALSE(get_result.HeaderIsPresent("Service-Worker-Allowed"));

    // COEP header does not make sense in non-document responses.
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Embedder-Policy"));

    // CSP header does not make sense in non-document responses
    // TODO(crbug.com/333078381): remove "Content-Security-Policy" header from
    // non-document responses and update this check.
    EXPECT_TRUE(get_result.HeaderIsPresent("Content-Security-Policy"));

    // COOP header does not make sense in non-document responses.
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Opener-Policy"));

    // Origin Trials header does not make sense in video resource responses.
    EXPECT_FALSE(get_result.HeaderIsPresent("Origin-Trial"));
  }
}

// Tests that request for background script returns a few expected response
// headers.
TEST_P(ExtensionProtocolsTest, BackgroundScriptRequestResponseHeaders) {
  const int manifest_version = GetParam();
  scoped_refptr<const Extension> extension =
      CreateTestResponseHeaderExtension(manifest_version);
  AddExtension(extension, false, false);

  {
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("background.js"),
                      network::mojom::RequestDestination::kServiceWorker);
    EXPECT_EQ(net::OK, get_result.result());

    // Check that cache-related headers are set.
    std::string etag = get_result.GetResponseHeaderByName("ETag");
    EXPECT_TRUE(base::StartsWith(etag, "\"", base::CompareCase::SENSITIVE));
    EXPECT_TRUE(base::EndsWith(etag, "\"", base::CompareCase::SENSITIVE));

    EXPECT_EQ("no-cache", get_result.GetResponseHeaderByName("Cache-Control"));

    // Background scripts are not web-accessible, so do not need CORS headers.
    EXPECT_FALSE(get_result.HeaderIsPresent("Access-Control-Allow-Origin"));
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Resource-Policy"));

    // Only background service worker script should be allowed to load as a
    // service worker.
    if (manifest_version == 3) {
      EXPECT_EQ("/",
                get_result.GetResponseHeaderByName("Service-Worker-Allowed"));
    } else {
      EXPECT_FALSE(get_result.HeaderIsPresent("Service-Worker-Allowed"));
    }

    // COEP header does not make sense in non-document responses.
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Embedder-Policy"));

    // Even though CSP is currently not respected for service workers, it
    // probably should be. We continue to send a CSP header for service worker
    // scripts for when this changes.
    // See also
    // https://github.com/w3c/webappsec-csp/issues/336#issuecomment-1274730655
    if (manifest_version == 3) {
      EXPECT_EQ("script-src 'self';",
                get_result.GetResponseHeaderByName("Content-Security-Policy"));
    } else {
      EXPECT_EQ(
          "script-src 'self' blob: filesystem:; object-src 'self' blob: "
          "filesystem:;",
          get_result.GetResponseHeaderByName("Content-Security-Policy"));
    }

    // COOP header does not make sense in non-document responses.
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Opener-Policy"));
  }
}

// Tests that request for background service worker returns Origin-Trial
// response header.
TEST_P(ExtensionProtocolsMV3Test, BackgroundScriptRequestResponseHeaders) {
  EXPECT_EQ(3, GetParam());
  scoped_refptr<const Extension> extension =
      CreateTestResponseHeaderExtension(GetParam());
  AddExtension(extension, false, false);

  {
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("background.js"),
                      network::mojom::RequestDestination::kServiceWorker);
    EXPECT_EQ(net::OK, get_result.result());

    // In MV3-style service workers origin trail tokens are served via service
    // worker Origin-Trial header.
    EXPECT_EQ(kTrialTokensHeaderValue,
              get_result.GetResponseHeaderByName("Origin-Trial"));
  }
}

// TODO(crbug.com/333078381): Add a test checking that:
// - when background.page or background.service_worker is specified requesting
//   generated background page fails
// - when no background is specified, requesting generated background page fails

TEST_P(ExtensionProtocolsTest, BackgroundPageRequestResponseHeaders) {
  const int manifest_version = GetParam();
  scoped_refptr<const Extension> extension =
      CreateTestResponseHeaderExtension(manifest_version);
  AddExtension(extension, false, false);

  {
    auto get_result = RequestOrLoad(
        extension->GetResourceURL(kGeneratedBackgroundPageFilename),
        network::mojom::RequestDestination::kDocument);
    EXPECT_EQ(net::OK, get_result.result());

    // Check that cache-related headers are omitted
    // TODO(crbug.com/333078381): consider adding these headers to generated
    // pages.
    EXPECT_FALSE(get_result.HeaderIsPresent("ETag"));
    EXPECT_FALSE(get_result.HeaderIsPresent("Cache-Control"));

    // Background pages are not web-accessible, so do not need CORS headers.
    EXPECT_FALSE(get_result.HeaderIsPresent("Access-Control-Allow-Origin"));
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Resource-Policy"));

    // Background page does not need to be loaded as a service worker.
    EXPECT_FALSE(get_result.HeaderIsPresent("Service-Worker-Allowed"));

    // Background page does not load cross-origin content so does not need COEP
    // header.
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Embedder-Policy"));

    if (manifest_version == 3) {
      EXPECT_EQ("script-src 'self';",
                get_result.GetResponseHeaderByName("Content-Security-Policy"));
    } else {
      EXPECT_EQ(
          "script-src 'self' blob: filesystem:; object-src 'self' blob: "
          "filesystem:;",
          get_result.GetResponseHeaderByName("Content-Security-Policy"));
    }

    // COOP header does not make sense in non-document responses.
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Opener-Policy"));
  }
}

// Tests that resources from imported module extensions get appropriately
// loaded with proper headers or rejected
TEST_P(ExtensionProtocolsTest, ModuleRequestResponseHeaders) {
  const int manifest_version = GetParam();
  scoped_refptr<const Extension> module_extension =
      CreateTestModuleResponseHeaderExtension(manifest_version);
  scoped_refptr<const Extension> importer_extension =
      CreateTestModuleImporterResponseHeaderExtension(manifest_version,
                                                      module_extension->id());
  AddExtension(module_extension, false, false);
  AddExtension(importer_extension, false, false);

  // Not imported id will fail.
  {
    auto get_result =
        RequestOrLoad(importer_extension->GetResourceURL(
                          "_modules/modaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/test.dat"),
                      network::mojom::RequestDestination::kDocument);
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result());
  }
  {
    auto get_result =
        RequestOrLoad(importer_extension->GetResourceURL(
                          "_modules/modaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/test.dat"),
                      network::mojom::RequestDestination::kServiceWorker);
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result());
  }

  // Imported resources get loaded with proper headers (inherited from
  // importer).
  {
    auto get_result =
        RequestOrLoad(importer_extension->GetResourceURL(
                          "_modules/" + module_extension->id() + "/test.dat"),
                      network::mojom::RequestDestination::kDocument);
    EXPECT_EQ(net::OK, get_result.result());

    // Check that cache-related headers are set.
    std::string etag = get_result.GetResponseHeaderByName("ETag");
    EXPECT_TRUE(base::StartsWith(etag, "\"", base::CompareCase::SENSITIVE));
    EXPECT_TRUE(base::EndsWith(etag, "\"", base::CompareCase::SENSITIVE));

    // Background pages are not web-accessible, so do not need CORS headers.
    EXPECT_FALSE(get_result.HeaderIsPresent("Access-Control-Allow-Origin"));
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Resource-Policy"));

    // Background page does not need to be loaded as a service worker.
    EXPECT_FALSE(get_result.HeaderIsPresent("Service-Worker-Allowed"));

    // Background page does not load cross-origin content so does not need COEP
    // header.
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Embedder-Policy"));

    if (manifest_version == 3) {
      EXPECT_EQ("script-src 'self';",
                get_result.GetResponseHeaderByName("Content-Security-Policy"));
    } else {
      EXPECT_EQ(
          "script-src 'self' blob: filesystem:; object-src 'self' blob: "
          "filesystem:;",
          get_result.GetResponseHeaderByName("Content-Security-Policy"));
    }

    // COOP header does not make sense in non-document responses.
    EXPECT_FALSE(get_result.HeaderIsPresent("Cross-Origin-Opener-Policy"));
  }
}

// Tests that request for background service worker returns Origin-Trial
// response header.
TEST_P(ExtensionProtocolsMV3Test, ModuleRequestResponseHeaders) {
  EXPECT_EQ(3, GetParam());
  const int manifest_version = GetParam();
  scoped_refptr<const Extension> module_extension =
      CreateTestModuleResponseHeaderExtension(manifest_version);
  scoped_refptr<const Extension> importer_extension =
      CreateTestModuleImporterResponseHeaderExtension(manifest_version,
                                                      module_extension->id());
  AddExtension(module_extension, false, false);
  AddExtension(importer_extension, false, false);

  // Imported resources get loaded with proper headers (inherited from
  // importer).
  {
    auto get_result =
        RequestOrLoad(importer_extension->GetResourceURL(
                          "_modules/" + module_extension->id() + "/test.dat"),
                      network::mojom::RequestDestination::kDocument);
    EXPECT_EQ(net::OK, get_result.result());

    // Origin-Trial header should contain trials inherited from importer.
    EXPECT_EQ(kTrialTokensHeaderValue,
              get_result.GetResponseHeaderByName("Origin-Trial"));
  }
}

TEST_P(ExtensionProtocolsTest, InvalidBackgroundScriptRequest) {
  const int manifest_version = GetParam();
  scoped_refptr<const Extension> extension =
      CreateTestResponseHeaderExtension(manifest_version);
  AddExtension(extension, false, false);

  // Requesting script from background key with invalid destination is
  // forbidden.
  std::vector<network::mojom::RequestDestination> destinations = {
      // TODO(crbug.com/333078381): carefully consider which other
      // request destinations should be allowed or blocked and update
      // this test
      network::mojom::RequestDestination::kJson,
      network::mojom::RequestDestination::kStyle,
      network::mojom::RequestDestination::kVideo,
  };
  if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    destinations.push_back(network::mojom::RequestDestination::kWorker);
  }
  for (network::mojom::RequestDestination destination : destinations) {
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("background.js"), destination);
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result()) << destination;
  }
}

// Tests that a URL request for main frame or subframe from an extension
// succeeds, but subresources fail. See http://crbug.com/312269.
TEST_P(ExtensionProtocolsTest, AllowFrameRequests) {
  scoped_refptr<const Extension> extension =
      CreateTestExtension("foo", false, GetParam());
  AddExtension(extension, false, false);

  // All MAIN_FRAME requests should succeed. SUB_FRAME requests that are not
  // explicitly listed in web_accessible_resources or same-origin to the parent
  // should not succeed.
  {
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("test.dat"),
                      network::mojom::RequestDestination::kDocument);
    EXPECT_EQ(net::OK, get_result.result());
  }

  // Subframe navigation requests are blocked in ExtensionNavigationThrottle
  // which isn't added in this unit test. This is tested in an integration test
  // in ExtensionResourceRequestPolicyTest.IframeNavigateToInaccessible.

  // And subresource types, such as media, should fail.
  {
    auto get_result = RequestOrLoad(extension->GetResourceURL("test.dat"),
                                    network::mojom::RequestDestination::kVideo);
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result());
  }
}

// Make sure requests for paths ending with a separator aren't allowed. See
// https://crbug.com/356878412.
TEST_P(ExtensionProtocolsTest, PathsWithTrailingSeparatorsAreNotAllowed) {
  base::FilePath extension_dir = GetTestPath("simple_with_file");
  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kInternal, Extension::NO_FLAGS,
      &error);
  ASSERT_NE(extension.get(), nullptr) << "error: " << error;

  // Loading "/file.html" should succeed.
  EXPECT_EQ(net::OK, DoRequestOrLoad(extension, "file.html").result());

  // Loading "/file.html/" should fail.
  base::FilePath relative_path =
      base::FilePath(FILE_PATH_LITERAL("file.html")).AsEndingWithSeparator();
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
            DoRequestOrLoad(extension, relative_path.AsUTF8Unsafe()).result());
}

// Make sure directories with an index.html file aren't serving the file, i.e.
// index.html doesn't get any special treatment.
TEST_P(ExtensionProtocolsTest, DirectoryWithIndexHtml) {
  base::FilePath extension_dir = GetTestPath("simple_with_index_html");
  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kInternal, Extension::NO_FLAGS,
      &error);
  ASSERT_NE(extension.get(), nullptr) << "error: " << error;

  // Loading "/test_dir" should fail.
  base::FilePath relative_path(FILE_PATH_LITERAL("test_dir"));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
            DoRequestOrLoad(extension, relative_path.AsUTF8Unsafe()).result());

  // Loading "/test_dir/" should fail.
  relative_path = relative_path.AsEndingWithSeparator();
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
            DoRequestOrLoad(extension, relative_path.AsUTF8Unsafe()).result());

  // Loading "/test_dir/index.html" explicitly should succeed.
  relative_path = relative_path.AppendASCII("index.html");
  EXPECT_EQ(net::OK,
            DoRequestOrLoad(extension, relative_path.AsUTF8Unsafe()).result());
}

TEST_P(ExtensionProtocolsTest, MetadataFolder) {
  base::FilePath extension_dir = GetTestPath("metadata_folder");
  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kInternal, Extension::NO_FLAGS,
      &error);
  ASSERT_NE(extension.get(), nullptr) << "error: " << error;

  // Loading "/test.html" should succeed.
  EXPECT_EQ(net::OK, DoRequestOrLoad(extension, "test.html").result());

  // Loading "/_metadata/verified_contents.json" should fail.
  base::FilePath relative_path =
      base::FilePath(kMetadataFolder).Append(kVerifiedContentsFilename);
  EXPECT_TRUE(base::PathExists(extension_dir.Append(relative_path)));
  EXPECT_NE(net::OK,
            DoRequestOrLoad(extension, relative_path.AsUTF8Unsafe()).result());

  // Loading "/_metadata/a.txt" should also fail.
  relative_path = base::FilePath(kMetadataFolder).AppendASCII("a.txt");
  EXPECT_TRUE(base::PathExists(extension_dir.Append(relative_path)));
  EXPECT_NE(net::OK,
            DoRequestOrLoad(extension, relative_path.AsUTF8Unsafe()).result());
}

// Tests that unreadable files and deleted files correctly go through
// ContentVerifyJob.
TEST_P(ExtensionProtocolsTest, VerificationSeenForFileAccessErrors) {
  // Unzip extension containing verification hashes to a temporary directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir.GetPath();
  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          GetContentVerifierTestPath().AppendASCII("source.zip"),
          unzipped_path);
  ASSERT_TRUE(extension.get());
  ExtensionId extension_id = extension->id();

  const std::string kJs("1024.js");
  base::FilePath kRelativePath(FILE_PATH_LITERAL("1024.js"));

  // Valid and readable 1024.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

  // chmod -r 1024.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    base::FilePath file_path = unzipped_path.AppendASCII(kJs);
    ASSERT_TRUE(base::MakeFileUnreadable(file_path));
    EXPECT_EQ(net::ERR_ACCESS_DENIED, DoRequestOrLoad(extension, kJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
    // NOTE: In production, hash mismatch would have disabled |extension|, but
    // since UnzipToDirAndLoadExtension() doesn't add the extension to
    // ExtensionRegistry, ChromeContentVerifierDelegate won't disable it.
    // TODO(lazyboy): We may want to update this to more closely reflect the
    // real flow.
  }

  // Delete 1024.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    base::FilePath file_path = unzipped_path.AppendASCII(kJs);
    ASSERT_TRUE(base::DieFileDie(file_path, false));
    EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
              DoRequestOrLoad(extension, kJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
  }
}

// Tests that zero byte files correctly go through ContentVerifyJob.
TEST_P(ExtensionProtocolsTest, VerificationSeenForZeroByteFile) {
  const std::string kEmptyJs("empty.js");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir.GetPath();

  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          GetContentVerifierTestPath().AppendASCII("source.zip"),
          unzipped_path);
  ASSERT_TRUE(extension.get());

  base::FilePath kRelativePath(FILE_PATH_LITERAL("empty.js"));
  ExtensionId extension_id = extension->id();

  // Sanity check empty.js.
  base::FilePath file_path = unzipped_path.AppendASCII(kEmptyJs);
  int64_t foo_file_size = -1;
  ASSERT_TRUE(base::GetFileSize(file_path, &foo_file_size));
  ASSERT_EQ(0, foo_file_size);

  // Request empty.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

  // chmod -r empty.js.
  // Unreadable empty file results in hash mismatch.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    ASSERT_TRUE(base::MakeFileUnreadable(file_path));
    EXPECT_EQ(net::ERR_ACCESS_DENIED,
              DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
  }

  // rm empty.js.
  // Deleted empty file results in hash mismatch.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    ASSERT_TRUE(base::DieFileDie(file_path, false));
    EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
              DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
  }
}

TEST_P(ExtensionProtocolsTest, VerifyScriptListedAsIcon) {
  const std::string kBackgroundJs("background.js");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir.GetPath();

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(DIR_TEST_DATA, &path));

  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          path.AppendASCII("content_hash_fetcher")
              .AppendASCII("manifest_mislabeled_script")
              .AppendASCII("source.zip"),
          unzipped_path);
  ASSERT_TRUE(extension.get());

  base::FilePath kRelativePath(FILE_PATH_LITERAL("background.js"));
  ExtensionId extension_id = extension->id();

  // Request background.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kBackgroundJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

  // Modify background.js and request it.
  {
    base::FilePath file_path = unzipped_path.AppendASCII("background.js");
    const std::string content = "new content";
    EXPECT_TRUE(base::WriteFile(file_path, content));

    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kBackgroundJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
  }
}

// Tests that mime types are properly set for returned extension resources.
TEST_P(ExtensionProtocolsTest, MimeTypesForKnownFiles) {
  TestExtensionDir test_dir;
  const int manifest_version = GetParam();
  constexpr char kManifestV2[] = R"(
      {
        "name": "Test Ext",
        "manifest_version": 2,
        "version": "1",
        "web_accessible_resources": ["*"]
      })";
  constexpr char kManifestV3[] = R"(
      {
        "name": "Test Ext",
        "manifest_version": 3,
        "version": "1",
        "web_accessible_resources": [{
          "resources": [ "*" ],
          "matches": [ "*://*/*" ]
        }]
      })";
  const char* kManifest = manifest_version == 3 ? kManifestV3 : kManifestV2;
  test_dir.WriteManifest(kManifest);
  base::Value::Dict manifest = base::test::ParseJsonDict(kManifest);
  ASSERT_FALSE(manifest.empty());

  test_dir.WriteFile(FILE_PATH_LITERAL("json_file.json"), "{}");
  test_dir.WriteFile(FILE_PATH_LITERAL("js_file.js"), "function() {}");

  base::FilePath unpacked_path = test_dir.UnpackedPath();
  ASSERT_TRUE(base::PathExists(unpacked_path.AppendASCII("json_file.json")));
  std::string error;
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetPath(unpacked_path)
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  ASSERT_TRUE(extension);

  AddExtension(extension.get(), false, false);

  struct {
    const char* file_name;
    const char* expected_mime_type;
  } test_cases[] = {
      {"json_file.json", "application/json"},
      {"js_file.js", "text/javascript"},
      {"mem_file.mem", ""},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.file_name);
    EXPECT_EQ(
        test_case.expected_mime_type,
        RequestOrLoad(extension->GetResourceURL(test_case.file_name),
                      network::mojom::RequestDestination::kEmpty)
            .GetResponseHeaderByName(net::HttpRequestHeaders::kContentType));
  }
}

// Tests that requests for extension resources (including the generated
// background page) are not aborted on system suspend.
TEST_P(ExtensionProtocolsTest, ExtensionRequestsNotAborted) {
  base::FilePath extension_dir =
      GetTestPath("common").AppendASCII("background_script");
  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kInternal, Extension::NO_FLAGS,
      &error);
  ASSERT_TRUE(extension.get()) << error;

  EnableSimulationOfSystemSuspendForRequests();

  // Request the generated background page. Ensure the request completes
  // successfully.
  EXPECT_EQ(net::OK,
            DoRequestOrLoad(extension.get(), kGeneratedBackgroundPageFilename)
                .result());

  // Request the background.js file. Ensure the request completes successfully.
  EXPECT_EQ(net::OK,
            DoRequestOrLoad(extension.get(), "background.js").result());
}

}  // namespace extensions
