// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/digital_asset_links/digital_asset_links_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kStatementList[] = R"(
[{
  "relation": ["other_relationship"],
  "target": {
    "namespace": "android_app",
    "package_name": "com.peter.trustedpetersactivity",
    "sha256_cert_fingerprints": [
      "FA:2A:03:CB:38:9C:F3:BE:28:E3:CA:7F:DA:2E:FA:4F:4A:96:F3:BC:45:2C:08:A2:16:A1:5D:FD:AB:46:BC:9D"
    ]
  }
}, {
  "relation": ["delegate_permission/common.handle_all_urls"],
  "target": {
    "namespace": "android_app",
    "package_name": "com.example.firstapp",
    "sha256_cert_fingerprints": [
      "64:2F:D4:BE:1C:4D:F8:36:2E:D3:50:C4:69:53:96:A1:3D:14:0A:23:AD:2F:BF:EB:6E:C6:E4:64:54:3B:34:C1"
    ]
  }
}]
)";

const char kDomain[] = "https://www.example.com";
const char kValidPackage[] = "com.example.firstapp";
const char kValidRelation[] = "delegate_permission/common.handle_all_urls";
const char kValidFingerprint[] =
    "64:2F:D4:BE:1C:4D:F8:36:2E:D3:50:C4:69:53:96:A1:3D:14:0A:23:AD:2F:BF:EB:"
    "6E:C6:E4:64:54:3B:34:C1";

}  // namespace

namespace digital_asset_links {
namespace {

class DigitalAssetLinksHandlerTest : public ::testing::Test {
 public:
  DigitalAssetLinksHandlerTest()
      : num_invocations_(0),
        result_(RelationshipCheckResult::SUCCESS),
        io_thread_(content::BrowserThread::IO,
                   base::ThreadTaskRunnerHandle::Get()) {}

  void OnRelationshipCheckComplete(RelationshipCheckResult result) {
    ++num_invocations_;
    result_ = result;
  }

 protected:
  void SetUp() override { num_invocations_ = 0; }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  void AddErrorResponse(net::Error error, int response_code) {
    request_url_ =
        test_url_loader_factory_.pending_requests()->at(0).request.url;

    auto response_head = network::mojom::URLResponseHead::New();
    std::string status_line =
        "HTTP/1.1 " + base::NumberToString(response_code) + " " +
        net::GetHttpReasonPhrase(
            static_cast<net::HttpStatusCode>(response_code));
    response_head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(status_line);
    test_url_loader_factory_.AddResponse(
        request_url_, std::move(response_head), "",
        network::URLLoaderCompletionStatus(error));

    base::RunLoop().RunUntilIdle();
  }

  void AddResponse(const std::string& response) {
    request_url_ =
        test_url_loader_factory_.pending_requests()->at(0).request.url;

    test_url_loader_factory_.AddResponse(request_url_.spec(), response,
                                         net::HTTP_OK);

    base::RunLoop().RunUntilIdle();
  }

  int num_invocations_;
  RelationshipCheckResult result_;
  GURL request_url_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  content::TestBrowserThread io_thread_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(DigitalAssetLinksHandlerTest);
};
}  // namespace

TEST_F(DigitalAssetLinksHandlerTest, CorrectAssetLinksUrl) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddResponse("");

  EXPECT_EQ(request_url_,
            "https://www.example.com/.well-known/assetlinks.json");
}

TEST_F(DigitalAssetLinksHandlerTest, PositiveResponse) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::SUCCESS);
}

TEST_F(DigitalAssetLinksHandlerTest, PackageMismatch) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, "evil.package", kValidFingerprint, kValidRelation);
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, SignatureMismatch) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, "66:66:66:66:66:66", kValidRelation);
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, RelationshipMismatch) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, "take_firstborn_child");
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, StatementIsolation) {
  // Ensure we don't merge separate statements together.
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, "other_relationship");
  AddResponse(kStatementList);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, BadAssetLinks_Empty) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddResponse("");

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, BadAssetLinks_NotList) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddResponse(R"({ "key": "value"})");

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, BadAssetLinks_StatementNotDict) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddResponse(R"([ [], [] ])");

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, BadAssetLinks_MissingFields) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddResponse(R"([ { "target" : {} } ])");

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, BadRequest) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddErrorResponse(net::OK, net::HTTP_BAD_REQUEST);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, NetworkError) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddErrorResponse(net::ERR_ABORTED, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::FAILURE);
}

TEST_F(DigitalAssetLinksHandlerTest, NetworkDisconnected) {
  DigitalAssetLinksHandler handler(/* web_contents= */ nullptr,
                                   GetSharedURLLoaderFactory());
  handler.CheckDigitalAssetLinkRelationship(
      base::BindOnce(&DigitalAssetLinksHandlerTest::OnRelationshipCheckComplete,
                     base::Unretained(this)),
      kDomain, kValidPackage, kValidFingerprint, kValidRelation);
  AddErrorResponse(net::ERR_INTERNET_DISCONNECTED, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_EQ(result_, RelationshipCheckResult::NO_CONNECTION);
}
}  // namespace digital_asset_links
