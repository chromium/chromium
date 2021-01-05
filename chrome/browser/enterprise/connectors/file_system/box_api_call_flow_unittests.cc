// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for all subclasses of BoxApiCallFlow.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

template <typename ApiCallMiniClass>
class BoxApiCallFlowTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ApiCallMiniClass> flow_;

  bool processed_success_ = false;
  std::string processed_folder_id_ = "default id";
};

////////////////////////////////////////////////////////////////////////////////
// FindUpstreamFolder
////////////////////////////////////////////////////////////////////////////////

class BoxFindUpstreamFolderApiCallFlowForTest
    : public BoxFindUpstreamFolderApiCallFlow {
 public:
  using BoxFindUpstreamFolderApiCallFlow::BoxFindUpstreamFolderApiCallFlow;
  using BoxFindUpstreamFolderApiCallFlow::CreateApiCallUrl;
  using BoxFindUpstreamFolderApiCallFlow::ProcessApiCallFailure;
  using BoxFindUpstreamFolderApiCallFlow::ProcessApiCallSuccess;

 private:
  DISALLOW_COPY_AND_ASSIGN(BoxFindUpstreamFolderApiCallFlowForTest);
};

class BoxFindUpstreamFolderApiCallFlowTest
    : public BoxApiCallFlowTest<BoxFindUpstreamFolderApiCallFlowForTest> {
 protected:
  void SetUp() override {
    flow_ = std::make_unique<BoxFindUpstreamFolderApiCallFlowForTest>(
        base::BindOnce(&BoxFindUpstreamFolderApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()));
  }

  void OnResponse(bool success,
                  int response_code,
                  const std::string& folder_id) {
    processed_success_ = success;
    processed_folder_id_ = folder_id;
  }

  base::WeakPtrFactory<BoxFindUpstreamFolderApiCallFlowTest> factory_{this};
};

TEST_F(BoxFindUpstreamFolderApiCallFlowTest, CreateApiCallUrl) {
  GURL url("https://api.box.com/2.0/search?type=folder&query=ChromeDownloads");
  ASSERT_EQ(flow_->CreateApiCallUrl(), url);
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_BAD_REQUEST);
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(processed_folder_id_, "");
}

class BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess
    : public BoxFindUpstreamFolderApiCallFlowTest {
 protected:
  void SetUp() override {
    BoxFindUpstreamFolderApiCallFlowTest::SetUp();
    head_ = network::CreateURLResponseHead(net::HTTP_OK);
  }
  data_decoder::test::InProcessDataDecoder decoder_;
  network::mojom::URLResponseHeadPtr head_;
};

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess, EmptyBody) {
  flow_->ProcessApiCallSuccess(head_.get(), std::make_unique<std::string>());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(processed_folder_id_, "");
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess,
       InvalidBody) {
  flow_->ProcessApiCallSuccess(head_.get(),
                               std::make_unique<std::string>("adgafdga"));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(processed_folder_id_, "");
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess,
       EmptyEntries) {
  std::string body(R"({
    "entries": [
        ]
  })");

  flow_->ProcessApiCallSuccess(head_.get(),
                               std::make_unique<std::string>(body));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_) << body;
  ASSERT_EQ(processed_folder_id_, "");
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess,
       ValidEntries) {
  std::string body(R"({
    "entries": [
      {
        "id": 12345,
        "etag": 1,
        "type": "folder",
        "sequence_id": 3,
        "name": "ChromeDownloads"
      }
    ]
  })");
  flow_->ProcessApiCallSuccess(head_.get(),
                               std::make_unique<std::string>(body));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(processed_folder_id_, "12345");
}

////////////////////////////////////////////////////////////////////////////////
// CreateUpstreamFolder
////////////////////////////////////////////////////////////////////////////////

class BoxCreateUpstreamFolderApiCallFlowForTest
    : public BoxCreateUpstreamFolderApiCallFlow {
 public:
  using BoxCreateUpstreamFolderApiCallFlow::BoxCreateUpstreamFolderApiCallFlow;
  using BoxCreateUpstreamFolderApiCallFlow::CreateApiCallBody;
  using BoxCreateUpstreamFolderApiCallFlow::CreateApiCallUrl;
  using BoxCreateUpstreamFolderApiCallFlow::IsExpectedSuccessCode;
  using BoxCreateUpstreamFolderApiCallFlow::ProcessApiCallFailure;
  using BoxCreateUpstreamFolderApiCallFlow::ProcessApiCallSuccess;

 private:
  DISALLOW_COPY_AND_ASSIGN(BoxCreateUpstreamFolderApiCallFlowForTest);
};

class BoxCreateUpstreamFolderApiCallFlowTest
    : public BoxApiCallFlowTest<BoxCreateUpstreamFolderApiCallFlowForTest> {
 protected:
  void SetUp() override {
    flow_ = std::make_unique<BoxCreateUpstreamFolderApiCallFlowForTest>(
        base::BindOnce(&BoxCreateUpstreamFolderApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()));
  }

  void OnResponse(bool success,
                  int response_code,
                  const std::string& folder_id) {
    processed_success_ = success;
    processed_folder_id_ = folder_id;
  }

  base::WeakPtrFactory<BoxCreateUpstreamFolderApiCallFlowTest> factory_{this};
};

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest, CreateApiCallUrl) {
  GURL url("https://api.box.com/2.0/folders");
  ASSERT_EQ(flow_->CreateApiCallUrl(), url);
}

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest, CreateApiCallBody) {
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(
      R"({"name":"ChromeDownloads","parent":{"id":"0"}})");
  ASSERT_EQ(body, expected_body);
}

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(201));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(403));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
}

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_BAD_REQUEST);
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(processed_folder_id_, "");
}

class BoxCreateUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess
    : public BoxCreateUpstreamFolderApiCallFlowTest {
 public:
  BoxCreateUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess()
      : head_(network::CreateURLResponseHead(net::HTTP_CREATED)) {}

 protected:
  data_decoder::test::InProcessDataDecoder decoder_;
  network::mojom::URLResponseHeadPtr head_;
};

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess, Normal) {
  std::string body(R"({
    "id": 12345,
    "type": "folder",
    "content_created_at": "2012-12-12T10:53:43-08:00",
    "content_modified_at": "2012-12-12T10:53:43-08:00",
    "created_at": "2012-12-12T10:53:43-08:00",
    "created_by": {
      "id": 11446498,
      "type": "user",
      "login": "ceo@example.com",
      "name": "Aaron Levie"
    },
    "description": "Legal contracts for the new ACME deal",
    "etag": 1,
    "expires_at": "2012-12-12T10:53:43-08:00",
    "folder_upload_email": {
      "access": "open",
      "email": "upload.Contracts.asd7asd@u.box.com"
    },
    "name": "ChromeDownloads",
    "owned_by": {
      "id": 11446498,
      "type": "user",
      "login": "ceo@example.com",
      "name": "Aaron Levie"
    },
    "parent": {
      "id": 0,
      "type": "folder",
      "etag": 1,
      "name": "",
      "sequence_id": 3
    }
  })");
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);
  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(processed_folder_id_, "12345");
}

}  // namespace enterprise_connectors
