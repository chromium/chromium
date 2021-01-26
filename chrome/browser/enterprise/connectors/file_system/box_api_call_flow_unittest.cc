// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for all subclasses of BoxApiCallFlow.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

template <typename ApiCallMiniClass>
class BoxApiCallFlowTest : public testing::Test {
 protected:
  std::unique_ptr<ApiCallMiniClass> flow_;

  bool processed_success_ = false;
  int response_code_ = -1;
};

template <typename ApiCallMiniClass>
class BoxFolderApiCallFlowTest : public BoxApiCallFlowTest<ApiCallMiniClass> {
 protected:
  void OnResponse(bool success,
                  int response_code,
                  const std::string& folder_id) {
    BoxApiCallFlowTest<ApiCallMiniClass>::processed_success_ = success;
    BoxApiCallFlowTest<ApiCallMiniClass>::response_code_ = response_code;
    processed_folder_id_ = folder_id;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
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
};

class BoxFindUpstreamFolderApiCallFlowTest
    : public BoxFolderApiCallFlowTest<BoxFindUpstreamFolderApiCallFlowForTest> {
 protected:
  void SetUp() override {
    flow_ = std::make_unique<BoxFindUpstreamFolderApiCallFlowForTest>(
        base::BindOnce(&BoxFindUpstreamFolderApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()));
  }

  base::WeakPtrFactory<BoxFindUpstreamFolderApiCallFlowTest> factory_{this};
};

TEST_F(BoxFindUpstreamFolderApiCallFlowTest, CreateApiCallUrl) {
  GURL url(kFileSystemBoxFindFolderUrl);
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
  flow_->ProcessApiCallSuccess(
      head_.get(), std::make_unique<std::string>(
                       kFileSystemBoxFindFolderResponseEmptyEntriesList));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
  ASSERT_EQ(processed_folder_id_, "");
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess, NoFolderId) {
  std::string body(R"({
    "entries": [
      {
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
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
  ASSERT_EQ(processed_folder_id_, "");
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess,
       EmptyFolderId) {
  std::string body(R"({
    "entries": [
      {
        "id": ,
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
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
  ASSERT_EQ(processed_folder_id_, "");
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess,
       IntegerFolderId) {
  flow_->ProcessApiCallSuccess(
      head_.get(),
      std::make_unique<std::string>(kFileSystemBoxFindFolderResponseBody));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
  ASSERT_EQ(processed_folder_id_, kFileSystemBoxFindFolderResponseFolderId);
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess,
       StringFolderId) {
  flow_->ProcessApiCallSuccess(
      head_.get(),
      std::make_unique<std::string>(kFileSystemBoxFindFolderResponseBody));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
  ASSERT_EQ(processed_folder_id_, kFileSystemBoxFindFolderResponseFolderId);
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess,
       FloatFolderId) {
  std::string body(R"({
    "entries": [
      {
        "id": 123.5,
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
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
  ASSERT_EQ(processed_folder_id_, "");
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
};

class BoxCreateUpstreamFolderApiCallFlowTest
    : public BoxFolderApiCallFlowTest<
          BoxCreateUpstreamFolderApiCallFlowForTest> {
 protected:
  void SetUp() override {
    flow_ = std::make_unique<BoxCreateUpstreamFolderApiCallFlowForTest>(
        base::BindOnce(&BoxCreateUpstreamFolderApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()));
  }

  base::WeakPtrFactory<BoxCreateUpstreamFolderApiCallFlowTest> factory_{this};
};

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest, CreateApiCallUrl) {
  GURL url(kFileSystemBoxCreateFolderUrl);
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
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);
  flow_->ProcessApiCallSuccess(
      http_head.get(),
      std::make_unique<std::string>(kFileSystemBoxCreateFolderResponseBody));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_EQ(processed_folder_id_, kFileSystemBoxCreateFolderResponseFolderId);
}

////////////////////////////////////////////////////////////////////////////////
// WholeFileUpload
////////////////////////////////////////////////////////////////////////////////

class BoxWholeFileUploadApiCallFlowForTest
    : public BoxWholeFileUploadApiCallFlow {
 public:
  using BoxWholeFileUploadApiCallFlow::BoxWholeFileUploadApiCallFlow;
  using BoxWholeFileUploadApiCallFlow::CreateApiCallBody;
  using BoxWholeFileUploadApiCallFlow::CreateApiCallBodyContentType;
  using BoxWholeFileUploadApiCallFlow::CreateApiCallUrl;
  using BoxWholeFileUploadApiCallFlow::IsExpectedSuccessCode;
  using BoxWholeFileUploadApiCallFlow::ProcessApiCallFailure;
  using BoxWholeFileUploadApiCallFlow::ProcessApiCallSuccess;
};

class BoxWholeFileUploadApiCallFlowTest
    : public BoxApiCallFlowTest<BoxWholeFileUploadApiCallFlowForTest> {
 protected:
  void SetUp() override {
    if (!temp_dir_.CreateUniqueTempDir()) {
      FAIL() << "Failed to create temporary directory for testing";
    }
    file_path_ = temp_dir_.GetPath().Append(file_name_);

    flow_ = std::make_unique<BoxWholeFileUploadApiCallFlowForTest>(
        base::BindOnce(&BoxWholeFileUploadApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        folder_id_, file_name_, file_path_);
  }

  void OnResponse(bool success, int response_code) {
    processed_success_ = success;
    response_code_ = response_code;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  std::string folder_id_{"13579"};
  const base::FilePath file_name_{
      FILE_PATH_LITERAL("box_whole_file_upload_test.txt")};
  base::FilePath file_path_;

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<BoxWholeFileUploadApiCallFlowTest> factory_{this};
};

TEST_F(BoxWholeFileUploadApiCallFlowTest, CreateApiCallUrl) {
  GURL url(kFileSystemBoxWholeFileUploadUrl);
  ASSERT_EQ(flow_->CreateApiCallUrl(), url);
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, CreateApiCallBodyAndContentType) {
  std::string content_type = flow_->CreateApiCallBodyContentType();
  std::string expected_type("multipart/form-data; boundary=");
  ASSERT_EQ(content_type.substr(0, expected_type.size()), expected_type);

  // Body format for multipart/form-data reference:
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
  // Request body fields reference:
  // https://developer.box.com/reference/post-files-content/
  std::string multipart_boundary =
      "--" + content_type.substr(expected_type.size());
  std::string expected_body(multipart_boundary + "\r\n");
  expected_body +=
      "Content-Disposition: form-data; name=\"attributes\"\r\n"
      "Content-Type: application/json\r\n\r\n"
      "{\"name\":\"";
  expected_body +=
      file_name_.AsUTF8Unsafe() +  // AsUTF8Unsafe() to compile on Windows
      "\","
      "\"parent\":{\"id\":\"" +
      folder_id_ + "\"}}\r\n";
  expected_body += multipart_boundary + "\r\n";
  expected_body += "Content-Disposition: form-data; name=\"file\"; filename=\"";
  expected_body +=
      file_name_.AsUTF8Unsafe() +  // AsUTF8Unsafe() to compile on Windows
      "\"\r\nContent-Type: text/plain\r\n\r\n\r\n";
  expected_body += multipart_boundary + "--\r\n";
  std::string body = flow_->CreateApiCallBody();
  ASSERT_EQ(body, expected_body);
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(201));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(403));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, ProcessApiCallSuccess) {
  // Create a temporary file to be deleted in ProcessApiCallSuccess().
  if (!base::WriteFile(file_path_, "BoxWholeFileUploadApiCallFlowTest")) {
    FAIL() << "Failed to create file " << file_path_;
  }

  std::string body;  // Dummy body since we don't read from body for now.
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_TRUE(processed_success_) << "Failed with file " << file_path_;
  ASSERT_FALSE(base::PathExists(file_path_));  // Make sure file is deleted.
}

TEST_F(BoxWholeFileUploadApiCallFlowTest,
       ProcessApiCallSuccess_NoFileToDelete) {
  std::string body;  // Dummy body since we don't read from body for now.
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);
  ASSERT_FALSE(base::PathExists(file_path_));  // Make sure file doesn't exist.

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_FALSE(processed_success_);  // Should fail file deletion.
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, ProcessApiCallFailure) {
  std::string body;  // Dummy body since we don't read from body here.
  auto http_head = network::CreateURLResponseHead(net::HTTP_CONFLICT);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->ProcessApiCallFailure(0, http_head.get(),
                               std::make_unique<std::string>(body));
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CONFLICT);
  ASSERT_FALSE(processed_success_);
}

class BoxWholeFileUploadApiCallFlowFileReadTest
    : public BoxWholeFileUploadApiCallFlowTest {
 public:
  BoxWholeFileUploadApiCallFlowFileReadTest()
      : url_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_factory_;
};

TEST_F(BoxWholeFileUploadApiCallFlowFileReadTest, GoodUpload) {
  if (!base::WriteFile(file_path_,
                       "BoxWholeFileUploadApiCallFlowFileReadTest")) {
    FAIL() << "Failed to create temporary file " << file_path_;
  }

  test_url_loader_factory_.AddResponse(
      kFileSystemBoxWholeFileUploadUrl,
      std::string(),  // Dummy body since we are not reading from body.
      net::HTTP_CREATED);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->Start(url_factory_, "dummytoken");
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_TRUE(processed_success_) << "Failed with file " << file_path_;
  ASSERT_FALSE(base::PathExists(file_path_));
}

TEST_F(BoxWholeFileUploadApiCallFlowFileReadTest, NoFile) {
  ASSERT_FALSE(base::PathExists(file_path_));

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->Start(url_factory_, "dummytoken");
  run_loop.Run();

  // There should be no HTTP response code, because it should already fail when
  // reading file, before making any actual API calls.
  ASSERT_EQ(response_code_, 0);
  ASSERT_FALSE(processed_success_);
}

}  // namespace enterprise_connectors
