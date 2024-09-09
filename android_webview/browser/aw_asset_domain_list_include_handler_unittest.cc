// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_asset_domain_list_include_handler.h"

#include "android_webview/browser/aw_app_defined_websites.h"
#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {
namespace {

const char kOriginIncludeUrl[] = "https://example.com/includestatements.json";

const char kOriginIncludeResponse[] =
    R"([
     {
       "relation": ["delegate_permission/common.handle_all_urls"],
       "target": {
         "namespace": "web",
         "site": "https://example.com"
       }
     },
     {
       "relation": ["delegate_permission/common.handle_all_urls"],
       "target": {
         "namespace": "web",
         "site": "https://assetsite.example"
       }
     }
     ])";

const char kOriginIncludeResponseWithDuplicate[] =
    R"([
     {
       "relation": ["delegate_permission/common.handle_all_urls"],
       "target": {
         "namespace": "web",
         "site": "https://example.com"
       }
     },
     {
       "relation": ["delegate_permission/common.handle_all_urls"],
       "target": {
         "namespace": "web",
         "site": "https://example.com"
       }
     }
     ])";

// Valid schema response that also includes android_app targets (which is not
// expected, but should be ignored).
// The sha256 fingerprint is not valid. It has been shortened to respect line
// width. It is only here as dummy data to be ignored.
const char kMixedTargetTypeIncludeResponse[] =
    R"([
     {
       "relation": ["delegate_permission/common.handle_all_urls"],
       "target": {
         "namespace": "android_app",
         "package_name": "com.example.notarealapp",
         "sha256_cert_fingerprints":
           ["14:6D:E9:83:C5:73:06::04:96:B2:3F:CF:44:E5"]
       }
     },
     {
       "relation": ["delegate_permission/common.handle_all_urls"],
       "target": {
         "namespace": "web",
         "site": "https://example.com"
       }
     }
     ])";

const char kMalformedIncludeResponse[] =
    "This is just a string and not a JSON list of objects.";

const char kOtherJsonObjectIncludeResponse[] = "{\"key\": \"value\"}";

class AssetDomainListIncludeHandlerTest : public testing::Test {
 public:
  AssetDomainListIncludeHandlerTest()
      : unit_under_test_(test_url_loader_factory_.GetSafeWeakWrapper()) {}

  void LoadDomainsBlocking(
      GURL url,
      AssetDomainListIncludeHandler::LoadCallback callback) {
    base::RunLoop runloop;
    unit_under_test_.LoadAppDefinedDomainIncludes(
        url, base::BindOnce(
                 [](base::OnceClosure done_closure,
                    AssetDomainListIncludeHandler::LoadCallback callback,
                    const std::vector<std::string>& origins) {
                   std::move(callback).Run(origins);
                   std::move(done_closure).Run();
                 },
                 runloop.QuitClosure(), std::move(callback)));
    runloop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  AssetDomainListIncludeHandler unit_under_test_;
};

TEST_F(AssetDomainListIncludeHandlerTest, CanLoadDomainList) {
  test_url_loader_factory_.AddResponse(kOriginIncludeUrl,
                                       kOriginIncludeResponse);
  LoadDomainsBlocking(
      GURL(kOriginIncludeUrl),
      base::BindOnce([](const std::vector<std::string>& origins) {
        EXPECT_THAT(origins, testing::UnorderedElementsAre(
                                 "example.com", "assetsite.example"));
      }));
}

TEST_F(AssetDomainListIncludeHandlerTest, CanLoadDomainListWithDuplicates) {
  test_url_loader_factory_.AddResponse(kOriginIncludeUrl,
                                       kOriginIncludeResponseWithDuplicate);
  LoadDomainsBlocking(
      GURL(kOriginIncludeUrl),
      base::BindOnce([](const std::vector<std::string>& origins) {
        EXPECT_THAT(origins, testing::UnorderedElementsAre("example.com"));
      }));
}

TEST_F(AssetDomainListIncludeHandlerTest, CanLoadDomainListWithIrrelevantData) {
  test_url_loader_factory_.AddResponse(kOriginIncludeUrl,
                                       kMixedTargetTypeIncludeResponse);
  LoadDomainsBlocking(
      GURL(kOriginIncludeUrl),
      base::BindOnce([](const std::vector<std::string>& origins) {
        EXPECT_THAT(origins, testing::ElementsAre("example.com"));
      }));
}

TEST_F(AssetDomainListIncludeHandlerTest, GetsEmptyListIfInvalidAddress) {
  auto head = network::mojom::URLResponseHead::New();
  network::URLLoaderCompletionStatus status(net::Error::ERR_ADDRESS_INVALID);
  test_url_loader_factory_.AddResponse(GURL(kOriginIncludeUrl), std::move(head),
                                       "", status);

  LoadDomainsBlocking(
      GURL(kOriginIncludeUrl),
      base::BindOnce([](const std::vector<std::string>& origins) {
        EXPECT_THAT(origins, testing::IsEmpty());
      }));
}

TEST_F(AssetDomainListIncludeHandlerTest, GetsEmptyListIfEmptyResponse) {
  test_url_loader_factory_.AddResponse(kOriginIncludeUrl, "");

  LoadDomainsBlocking(
      GURL(kOriginIncludeUrl),
      base::BindOnce([](const std::vector<std::string>& origins) {
        EXPECT_THAT(origins, testing::IsEmpty());
      }));
}

TEST_F(AssetDomainListIncludeHandlerTest, GetsEmptyListIfMalformedResponse) {
  test_url_loader_factory_.AddResponse(kOriginIncludeUrl,
                                       kMalformedIncludeResponse);

  LoadDomainsBlocking(
      GURL(kOriginIncludeUrl),
      base::BindOnce([](const std::vector<std::string>& origins) {
        EXPECT_THAT(origins, testing::IsEmpty());
      }));
}
TEST_F(AssetDomainListIncludeHandlerTest, GetsEmptyListIfUnexpectedJSONSchema) {
  test_url_loader_factory_.AddResponse(kOriginIncludeUrl,
                                       kOtherJsonObjectIncludeResponse);

  LoadDomainsBlocking(
      GURL(kOriginIncludeUrl),
      base::BindOnce([](const std::vector<std::string>& origins) {
        EXPECT_THAT(origins, testing::IsEmpty());
      }));
}
}  // namespace

}  // namespace android_webview
