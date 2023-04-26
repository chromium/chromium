// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
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
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "extensions/common/identifiability_metrics.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/scoped_identifiability_test_sample_collector.h"

using extensions::ExtensionRegistry;
using network::mojom::URLLoader;
using testing::_;
using testing::StrictMock;

namespace extensions {
namespace {

// Default extension id to use for extension generation when none is set.
constexpr char kEmptyExtensionId[] = "";

base::FilePath GetTestPath(const std::string& name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  return path.AppendASCII("extensions").AppendASCII(name);
}

base::FilePath GetContentVerifierTestPath() {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(extensions::DIR_TEST_DATA, &path));
  return path.AppendASCII("content_hash_fetcher")
      .AppendASCII("different_sized_files");
}

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             bool incognito_split_mode,
                                             const ExtensionId& extension_id) {
  auto manifest = base::Value::Dict().Set("name", name);
  manifest.Set("version", "1");
  manifest.Set("manifest_version", 2);
  manifest.Set("incognito", incognito_split_mode ? "split" : "spanning");

  base::FilePath path = GetTestPath("response_headers");

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, mojom::ManifestLocation::kInternal, manifest,
                        Extension::NO_FLAGS, extension_id, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             bool incognito_split_mode) {
  return CreateTestExtension(name, incognito_split_mode, kEmptyExtensionId);
}

scoped_refptr<Extension> CreateWebStoreExtension() {
  base::Value::Dict manifest =
      base::Value::Dict()
          .Set("name", "WebStore")
          .Set("version", "1")
          .Set("manifest_version", 2)
          .Set("icons", base::Value::Dict().Set("16", "webstore_icon_16.png"))
          .Set("web_accessible_resources",
               base::Value::List().Append("webstore_icon_16.png"));

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_RESOURCES, &path));
  path = path.AppendASCII("web_store");

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, mojom::ManifestLocation::kComponent, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

scoped_refptr<const Extension> CreateTestResponseHeaderExtension() {
  return ExtensionBuilder("An extension with web-accessible resources")
      .SetManifestKey("web_accessible_resources",
                      base::Value::List().Append("test.dat"))
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

  int result() const { return result_; }

 private:
  network::mojom::URLResponseHeadPtr response_;
  int result_;
};

}  // namespace

// This test lives in src/chrome instead of src/extensions because it tests
// functionality delegated back to Chrome via ChromeExtensionsBrowserClient.
// See chrome/browser/extensions/chrome_url_request_util.cc.
class ExtensionProtocolsTestBase : public testing::Test {
 public:
  explicit ExtensionProtocolsTestBase(bool force_incognito)
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        rvh_test_enabler_(new content::RenderViewHostTestEnabler()),
        force_incognito_(force_incognito),
        test_ukm_id_(ukm::SourceIdObj::New()) {}

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
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(browser_context()))
        ->set_content_verifier(content_verifier_.get());
  }

  void TearDown() override {
    loader_factory_.reset();
    content_verifier_->Shutdown();
    // Shut down the PowerMonitor if initialized.
    base::PowerMonitor::ShutdownForTesting();
  }

  void SetProtocolHandler(bool is_incognito) {
    loader_factory_.Bind(extensions::CreateExtensionNavigationURLLoaderFactory(
        browser_context(), test_ukm_id_, false));
  }

  GetResult RequestOrLoad(const GURL& url,
                          network::mojom::RequestDestination destination) {
    return LoadURL(url, destination);
  }

  void AddExtension(const scoped_refptr<const Extension>& extension,
                    bool incognito_enabled,
                    bool notifications_disabled) {
    EXPECT_TRUE(extension_registry()->AddEnabled(extension));
    ExtensionPrefs::Get(browser_context())
        ->SetIsIncognitoEnabled(extension->id(), incognito_enabled);
  }

  void RemoveExtension(const scoped_refptr<const Extension>& extension,
                       const UnloadedExtensionReason reason) {
    EXPECT_TRUE(extension_registry()->RemoveEnabled(extension->id()));
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

  void AddExtensionAndPerformResourceLoad(const ExtensionId& extension_id) {
    // Register a non-incognito extension protocol handler.
    SetProtocolHandler(false);

    scoped_refptr<Extension> extension =
        CreateTestExtension("foo", false, extension_id);
    AddExtension(extension, false, false);
    ASSERT_EQ(extension->id(), extension_id);

    // Load the extension.
    {
      auto get_result =
          RequestOrLoad(extension->GetResourceURL("test.dat"),
                        network::mojom::RequestDestination::kDocument);
      EXPECT_EQ(net::OK, get_result.result());
    }
  }

  void ExpectExtensionAccessResult(
      scoped_refptr<Extension> extension,
      const std::vector<
          blink::test::ScopedIdentifiabilityTestSampleCollector::Entry>&
          entries,
      ExtensionResourceAccessResult expected) {
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(test_ukm_id_.ToInt64(), entries[0].source);
    ASSERT_EQ(1u, entries[0].metrics.size());
    EXPECT_EQ(blink::IdentifiableSurface::FromTypeAndToken(
                  blink::IdentifiableSurface::Type::kExtensionFileAccess,
                  base::as_bytes(base::make_span(extension->id()))),
              entries[0].metrics[0].surface);
    EXPECT_EQ(blink::IdentifiableToken(expected), entries[0].metrics[0].value);
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

  content::WebContents* web_contents() { return contents_.get(); }

  content::RenderFrameHost* main_rfh() {
    return web_contents()->GetPrimaryMainFrame();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<content::WebContents> contents_;
  const bool force_incognito_;
  const ukm::SourceIdObj test_ukm_id_;

  absl::optional<base::test::ScopedPowerMonitorTestSource>
      power_monitor_source_;
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

// Tests that making a chrome-extension request in an incognito context is
// only allowed under the right circumstances (if the extension is allowed
// in incognito, and it's either a non-main-frame request or a split-mode
// extension).
TEST_F(ExtensionProtocolsIncognitoTest, IncognitoRequest) {
  // Register an incognito extension protocol handler.
  SetProtocolHandler(true);

  struct TestCase {
    // Inputs.
    std::string name;
    bool incognito_split_mode;
    bool incognito_enabled;

    // Expected results.
    bool should_allow_main_frame_load;
    bool should_allow_sub_frame_load;
  } cases[] = {
      {"spanning disabled", false, false, false, false},
      {"split disabled", true, false, false, false},
      {"spanning enabled", false, true, false, false},
      {"split enabled", true, true, true, false},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    scoped_refptr<Extension> extension =
        CreateTestExtension(cases[i].name, cases[i].incognito_split_mode);
    AddExtension(extension, cases[i].incognito_enabled, false);

    // First test a main frame request.
    {
      blink::test::ScopedIdentifiabilityTestSampleCollector metrics;

      // It doesn't matter that the resource doesn't exist. If the resource
      // is blocked, we should see BLOCKED_BY_CLIENT. Otherwise, the request
      // should just fail because the file doesn't exist.
      auto get_result =
          RequestOrLoad(extension->GetResourceURL("404.html"),
                        network::mojom::RequestDestination::kDocument);

      if (cases[i].should_allow_main_frame_load) {
        EXPECT_EQ(net::ERR_FILE_NOT_FOUND, get_result.result())
            << cases[i].name;
      } else {
        EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result())
            << cases[i].name;
      }

      // Either way it's a failure to the outside.
      ExpectExtensionAccessResult(extension, metrics.entries(),
                                  ExtensionResourceAccessResult::kFailure);
    }

    // Subframe navigation requests are blocked in ExtensionNavigationThrottle
    // which isn't added in this unit test. This is tested in an integration
    // test in ExtensionResourceRequestPolicyTest.IframeNavigateToInaccessible.
    RemoveExtension(extension, UnloadedExtensionReason::UNINSTALL);
  }
}

void CheckForContentLengthHeader(const GetResult& get_result) {
  std::string content_length = get_result.GetResponseHeaderByName(
      net::HttpRequestHeaders::kContentLength);

  EXPECT_FALSE(content_length.empty());
  int length_value = 0;
  EXPECT_TRUE(base::StringToInt(content_length, &length_value));
  EXPECT_GT(length_value, 0);
}

// Tests getting a resource for a component extension works correctly, both when
// the extension is enabled and when it is disabled.
TEST_F(ExtensionProtocolsTest, ComponentResourceRequest) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<Extension> extension = CreateWebStoreExtension();
  AddExtension(extension, false, false);

  // First test it with the extension enabled.
  {
    blink::test::ScopedIdentifiabilityTestSampleCollector metrics;

    auto get_result =
        RequestOrLoad(extension->GetResourceURL("webstore_icon_16.png"),
                      network::mojom::RequestDestination::kVideo);
    EXPECT_EQ(net::OK, get_result.result());
    CheckForContentLengthHeader(get_result);
    EXPECT_EQ("image/png", get_result.GetResponseHeaderByName(
                               net::HttpRequestHeaders::kContentType));

    ExpectExtensionAccessResult(extension, metrics.entries(),
                                ExtensionResourceAccessResult::kSuccess);
  }

  // And then test it with the extension disabled.
  RemoveExtension(extension, UnloadedExtensionReason::DISABLE);
  {
    blink::test::ScopedIdentifiabilityTestSampleCollector metrics;

    auto get_result =
        RequestOrLoad(extension->GetResourceURL("webstore_icon_16.png"),
                      network::mojom::RequestDestination::kVideo);
    EXPECT_EQ(net::OK, get_result.result());
    CheckForContentLengthHeader(get_result);
    EXPECT_EQ("image/png", get_result.GetResponseHeaderByName(
                               net::HttpRequestHeaders::kContentType));

    ExpectExtensionAccessResult(extension, metrics.entries(),
                                ExtensionResourceAccessResult::kSuccess);
  }
}

// Tests that a URL request for resource from an extension returns a few
// expected response headers.
TEST_F(ExtensionProtocolsTest, ResourceRequestResponseHeaders) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<const Extension> extension =
      CreateTestResponseHeaderExtension();
  AddExtension(extension, false, false);

  {
    auto get_result = RequestOrLoad(extension->GetResourceURL("test.dat"),
                                    network::mojom::RequestDestination::kVideo);
    EXPECT_EQ(net::OK, get_result.result());

    // Check that cache-related headers are set.
    std::string etag = get_result.GetResponseHeaderByName("ETag");
    EXPECT_TRUE(base::StartsWith(etag, "\"", base::CompareCase::SENSITIVE));
    EXPECT_TRUE(base::EndsWith(etag, "\"", base::CompareCase::SENSITIVE));

    std::string revalidation_header =
        get_result.GetResponseHeaderByName("cache-control");
    EXPECT_EQ("no-cache", revalidation_header);

    // We set test.dat as web-accessible, so it should have a CORS header.
    std::string access_control =
        get_result.GetResponseHeaderByName("Access-Control-Allow-Origin");
    EXPECT_EQ("*", access_control);
  }
}

// Tests that a URL request for main frame or subframe from an extension
// succeeds, but subresources fail. See http://crbug.com/312269.
TEST_F(ExtensionProtocolsTest, AllowFrameRequests) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<Extension> extension = CreateTestExtension("foo", false);
  AddExtension(extension, false, false);

  // All MAIN_FRAME requests should succeed. SUB_FRAME requests that are not
  // explicitly listed in web_accessible_resources or same-origin to the parent
  // should not succeed.
  {
    blink::test::ScopedIdentifiabilityTestSampleCollector metrics;

    auto get_result =
        RequestOrLoad(extension->GetResourceURL("test.dat"),
                      network::mojom::RequestDestination::kDocument);
    EXPECT_EQ(net::OK, get_result.result());

    ExpectExtensionAccessResult(extension, metrics.entries(),
                                ExtensionResourceAccessResult::kSuccess);
  }

  // Subframe navigation requests are blocked in ExtensionNavigationThrottle
  // which isn't added in this unit test. This is tested in an integration test
  // in ExtensionResourceRequestPolicyTest.IframeNavigateToInaccessible.

  // And subresource types, such as media, should fail.
  {
    blink::test::ScopedIdentifiabilityTestSampleCollector metrics;

    auto get_result = RequestOrLoad(extension->GetResourceURL("test.dat"),
                                    network::mojom::RequestDestination::kVideo);
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result());

    ExpectExtensionAccessResult(extension, metrics.entries(),
                                ExtensionResourceAccessResult::kFailure);
  }
}

TEST_F(ExtensionProtocolsTest, MetadataFolder) {
  SetProtocolHandler(false);

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
TEST_F(ExtensionProtocolsTest, VerificationSeenForFileAccessErrors) {
  SetProtocolHandler(false);

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

    content_verifier_->OnExtensionLoaded(browser_context(), extension.get());
    // Wait for PostTask to ContentVerifierIOData::AddData() to finish.
    content::RunAllPendingInMessageLoop();

    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

#if !BUILDFLAG(IS_FUCHSIA)  // Fuchsia does not support file permissions.
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
#endif  // !BUILDFLAG(IS_FUCHSIA)

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
TEST_F(ExtensionProtocolsTest, VerificationSeenForZeroByteFile) {
  SetProtocolHandler(false);

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

    content_verifier_->OnExtensionLoaded(browser_context(), extension.get());
    // Wait for PostTask to ContentVerifierIOData::AddData() to finish.
    content::RunAllPendingInMessageLoop();

    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

#if !BUILDFLAG(IS_FUCHSIA)  // Fuchsia does not support file permissions.
  // chmod -r empty.js.
  // Unreadable empty file doesn't generate hash mismatch. Note that this is the
  // current behavior of ContentVerifyJob.
  // TODO(lazyboy): The behavior is probably incorrect.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    ASSERT_TRUE(base::MakeFileUnreadable(file_path));
    EXPECT_EQ(net::ERR_ACCESS_DENIED,
              DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }
#endif  // !BUILDFLAG(IS_FUCHSIA)

  // rm empty.js.
  // Deleted empty file doesn't generate hash mismatch. Note that this is the
  // current behavior of ContentVerifyJob.
  // TODO(lazyboy): The behavior is probably incorrect.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    ASSERT_TRUE(base::DieFileDie(file_path, false));
    EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
              DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }
}

TEST_F(ExtensionProtocolsTest, VerifyScriptListedAsIcon) {
  SetProtocolHandler(false);

  const std::string kBackgroundJs("background.js");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir.GetPath();

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(extensions::DIR_TEST_DATA, &path));

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

    content_verifier_->OnExtensionLoaded(browser_context(), extension.get());
    // Wait for PostTask to ContentVerifierIOData::AddData() to finish.
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kBackgroundJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

  // Modify background.js and request it.
  {
    base::FilePath file_path = unzipped_path.AppendASCII("background.js");
    const std::string content = "new content";
    EXPECT_TRUE(base::WriteFile(file_path, content));
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);

    content_verifier_->OnExtensionLoaded(browser_context(), extension.get());
    // Wait for PostTask to ContentVerifierIOData::AddData() to finish.
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kBackgroundJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
  }
}

// Tests that mime types are properly set for returned extension resources.
TEST_F(ExtensionProtocolsTest, MimeTypesForKnownFiles) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  TestExtensionDir test_dir;
  constexpr char kManifest[] = R"(
      {
        "name": "Test Ext",
        "description": "A test extension",
        "manifest_version": 2,
        "version": "0.1",
        "web_accessible_resources": ["*"]
      })";
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
    auto result = RequestOrLoad(extension->GetResourceURL(test_case.file_name),
                                network::mojom::RequestDestination::kEmpty);
    EXPECT_EQ(
        test_case.expected_mime_type,
        result.GetResponseHeaderByName(net::HttpRequestHeaders::kContentType));
  }
}

// Tests that requests for extension resources (including the generated
// background page) are not aborted on system suspend.
TEST_F(ExtensionProtocolsTest, ExtensionRequestsNotAborted) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

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
