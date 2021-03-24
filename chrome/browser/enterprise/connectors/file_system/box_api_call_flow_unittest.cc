// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for all subclasses of BoxApiCallFlow.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "net/base/mime_util.h"
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
  ASSERT_EQ(response_code_, net::HTTP_BAD_REQUEST);
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
  std::string mime_type;
  net::GetMimeTypeFromExtension(FILE_PATH_LITERAL("txt"), &mime_type);
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
      "\"\r\nContent-Type: " + mime_type + "\r\n\r\n\r\n";
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

  std::string body;  // Placeholder body since we don't read from body for now.
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
  std::string body;  // Placeholder body since we don't read from body for now.
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
  std::string body;  // Placeholder body since we don't read from body here.
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
      std::string(),  // Placeholder body since we are not reading from body.
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

////////////////////////////////////////////////////////////////////////////////
// CreateUploadSession
////////////////////////////////////////////////////////////////////////////////

class BoxCreateUploadSessionApiCallFlowForTest
    : public BoxCreateUploadSessionApiCallFlow {
 public:
  using BoxCreateUploadSessionApiCallFlow::BoxCreateUploadSessionApiCallFlow;
  using BoxCreateUploadSessionApiCallFlow::CreateApiCallBody;
  using BoxCreateUploadSessionApiCallFlow::CreateApiCallUrl;
  using BoxCreateUploadSessionApiCallFlow::IsExpectedSuccessCode;
  using BoxCreateUploadSessionApiCallFlow::ProcessApiCallFailure;
  using BoxCreateUploadSessionApiCallFlow::ProcessApiCallSuccess;
};

class BoxCreateUploadSessionApiCallFlowTest
    : public BoxApiCallFlowTest<BoxCreateUploadSessionApiCallFlowForTest> {
 protected:
  void SetUp() override {
    flow_ = std::make_unique<BoxCreateUploadSessionApiCallFlowForTest>(
        base::BindOnce(&BoxCreateUploadSessionApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        /*folder_id = */ "13579", /*file_size = */ 60 * 1024 * 1024,
        /*file_name = */
        base::FilePath{FILE_PATH_LITERAL("box_chunked_upload_test.txt")});
  }

  void OnResponse(bool success,
                  int response_code,
                  base::Value session_endpoints) {
    processed_success_ = success;
    response_code_ = response_code;
    if (success) {
      session_upload_endpoint_ =
          session_endpoints.FindPath("upload_part")->GetString();
      session_abort_endpoint_ =
          session_endpoints.FindPath("abort")->GetString();
      session_commit_endpoint_ =
          session_endpoints.FindPath("commit")->GetString();
    }
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  std::string session_upload_endpoint_;
  std::string session_abort_endpoint_;
  std::string session_commit_endpoint_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder decoder_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<BoxCreateUploadSessionApiCallFlowTest> factory_{this};
};

TEST_F(BoxCreateUploadSessionApiCallFlowTest, CreateApiCallUrl) {
  GURL url(kFileSystemBoxChunkedUploadCreateSessionUrl);
  ASSERT_EQ(flow_->CreateApiCallUrl(), url);
}

TEST_F(BoxCreateUploadSessionApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(201));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(403));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
}

TEST_F(BoxCreateUploadSessionApiCallFlowTest, CreateApiCallBody) {
  std::string body = flow_->CreateApiCallBody();
  std::string expected_body(R"({"file_name":"box_chunked_upload_test.txt")");
  expected_body += R"(,"file_size":62914560,"folder_id":"13579"})";
  ASSERT_EQ(body, expected_body);
}

TEST_F(BoxCreateUploadSessionApiCallFlowTest, ProcessApiCallSuccess) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  flow_->ProcessApiCallSuccess(
      http_head.get(), std::make_unique<std::string>(
                           kFileSystemBoxCreateUploadSessionResponseBody));
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_TRUE(processed_success_);
  EXPECT_EQ(session_upload_endpoint_, kFileSystemBoxChunkedUploadSessionUrl);
  EXPECT_EQ(session_abort_endpoint_, kFileSystemBoxChunkedUploadSessionUrl);
  EXPECT_EQ(session_commit_endpoint_, kFileSystemBoxChunkedUploadCommitUrl);
}

TEST_F(BoxCreateUploadSessionApiCallFlowTest,
       ProcessApiCallSuccess_ParseFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  flow_->ProcessApiCallSuccess(
      http_head.get(),
      std::make_unique<std::string>());  // Empty body for parse failure.
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_FALSE(processed_success_);
}

TEST_F(BoxCreateUploadSessionApiCallFlowTest,
       ProcessApiCallSuccess_NoEndpoints) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);
  std::string body(R"({
    "id": "F971964745A5CD0C001BBE4E58196BFD",
    "type": "upload_session",
    "session_expires_at": "2012-12-12T10:53:43-08:00",
    "total_parts": 1000
  })");

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_FALSE(processed_success_);
}

TEST_F(BoxCreateUploadSessionApiCallFlowTest, ProcessApiCallFailure) {
  std::string body("item_name_invalid");
  auto http_head = network::CreateURLResponseHead(net::HTTP_BAD_REQUEST);

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->ProcessApiCallFailure(0, http_head.get(),
                               std::make_unique<std::string>(body));
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_BAD_REQUEST);
  ASSERT_FALSE(processed_success_);
}

////////////////////////////////////////////////////////////////////////////////
// PartFileUpload
////////////////////////////////////////////////////////////////////////////////

class BoxPartFileUploadApiCallFlowForTest
    : public BoxPartFileUploadApiCallFlow {
 public:
  using BoxPartFileUploadApiCallFlow::BoxPartFileUploadApiCallFlow;
  using BoxPartFileUploadApiCallFlow::CreateApiCallBody;
  using BoxPartFileUploadApiCallFlow::CreateApiCallBodyContentType;
  using BoxPartFileUploadApiCallFlow::CreateApiCallHeaders;
  using BoxPartFileUploadApiCallFlow::CreateApiCallUrl;
  using BoxPartFileUploadApiCallFlow::CreateFileDigest;
  using BoxPartFileUploadApiCallFlow::GetRequestTypeForBody;
  using BoxPartFileUploadApiCallFlow::IsExpectedSuccessCode;
  using BoxPartFileUploadApiCallFlow::ProcessApiCallFailure;
  using BoxPartFileUploadApiCallFlow::ProcessApiCallSuccess;
};

class BoxPartFileUploadApiCallFlowTest
    : public BoxApiCallFlowTest<BoxPartFileUploadApiCallFlowForTest> {
 protected:
  BoxPartFileUploadApiCallFlowTest()
      : file_content_("random file part content"),
        expected_sha_(
            BoxPartFileUploadApiCallFlow::CreateFileDigest(file_content_)),
        expected_content_range_(base::StringPrintf("bytes 0-%zu/%zu",
                                                   file_content_.size(),
                                                   file_content_.size())) {}

  void SetUp() override {
    flow_ = std::make_unique<BoxPartFileUploadApiCallFlowForTest>(
        base::BindOnce(&BoxPartFileUploadApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        kFileSystemBoxChunkedUploadSessionUrl, file_content_, 0,
        file_content_.size(), file_content_.size());
  }

  void OnResponse(bool success, int response_code, base::Value) {
    processed_success_ = success;
    response_code_ = response_code;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  const std::string file_content_;
  const std::string expected_sha_;
  const std::string expected_content_range_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder decoder_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<BoxPartFileUploadApiCallFlowTest> factory_{this};
};

TEST_F(BoxPartFileUploadApiCallFlowTest, CreateApiCallUrl) {
  ASSERT_EQ(flow_->CreateApiCallUrl(), kFileSystemBoxChunkedUploadSessionUrl);
}

TEST_F(BoxPartFileUploadApiCallFlowTest, CreateApiCallHeaders) {
  net::HttpRequestHeaders headers = flow_->CreateApiCallHeaders();
  std::string digest;
  headers.GetHeader("digest", &digest);
  base::Base64Decode(digest, &digest);
  EXPECT_EQ(digest, expected_sha_);

  std::string content_range;
  headers.GetHeader("content-range", &content_range);
  EXPECT_EQ(content_range, expected_content_range_) << headers.ToString();
}

TEST_F(BoxPartFileUploadApiCallFlowTest, CreateApiCallBody) {
  std::string body = flow_->CreateApiCallBody();
  ASSERT_EQ(body, file_content_);
}

TEST_F(BoxPartFileUploadApiCallFlowTest, CreateApiCallBodyContentType) {
  std::string type = flow_->CreateApiCallBodyContentType();
  ASSERT_EQ(type, "application/octet-stream");
}

TEST_F(BoxPartFileUploadApiCallFlowTest, GetRequestTypeForBody) {
  std::string type = flow_->GetRequestTypeForBody(file_content_);
  ASSERT_EQ(type, "PUT");
}

TEST_F(BoxPartFileUploadApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(200));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(201));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(202));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(412));
}

TEST_F(BoxPartFileUploadApiCallFlowTest, ProcessApiCallSuccess) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_OK);
  std::string body(R"({
    "part": {
    "offset": 16777216,
    "part_id": "6F2D3486",
    "sha1": "134b65991ed521fcfe4724b7d814ab8ded5185dc",
    "size": 3222784
  }
  })");
  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
}

TEST_F(BoxPartFileUploadApiCallFlowTest, ProcessApiCallSuccess_ParseError) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_OK);
  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
}

TEST_F(BoxPartFileUploadApiCallFlowTest, ProcessApiCallSuccess_EmptyResponse) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_OK);
  flow_->ProcessApiCallSuccess(
      http_head.get(), std::make_unique<std::string>(kEmptyResponseBody));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
}

TEST_F(BoxPartFileUploadApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CONFLICT);
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CONFLICT);
}

////////////////////////////////////////////////////////////////////////////////
// AbortUploadSession
////////////////////////////////////////////////////////////////////////////////
class BoxAbortUploadSessionApiCallFlowForTest
    : public BoxAbortUploadSessionApiCallFlow {
 public:
  using BoxAbortUploadSessionApiCallFlow::BoxAbortUploadSessionApiCallFlow;
  using BoxAbortUploadSessionApiCallFlow::CreateApiCallBody;
  using BoxAbortUploadSessionApiCallFlow::CreateApiCallUrl;
  using BoxAbortUploadSessionApiCallFlow::GetRequestTypeForBody;
  using BoxAbortUploadSessionApiCallFlow::IsExpectedSuccessCode;
  using BoxAbortUploadSessionApiCallFlow::ProcessApiCallFailure;
  using BoxAbortUploadSessionApiCallFlow::ProcessApiCallSuccess;
};

class BoxAbortUploadSessionApiCallFlowTest
    : public BoxApiCallFlowTest<BoxAbortUploadSessionApiCallFlowForTest> {
  void SetUp() override {
    flow_ = std::make_unique<BoxAbortUploadSessionApiCallFlowForTest>(
        base::BindOnce(&BoxAbortUploadSessionApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        kFileSystemBoxChunkedUploadSessionUrl);
  }

  void OnResponse(bool success, int response_code) {
    processed_success_ = success;
    response_code_ = response_code;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<BoxAbortUploadSessionApiCallFlowTest> factory_{this};
};

TEST_F(BoxAbortUploadSessionApiCallFlowTest, CreateApiCallUrl) {
  ASSERT_EQ(flow_->CreateApiCallUrl(), kFileSystemBoxChunkedUploadSessionUrl);
}

TEST_F(BoxAbortUploadSessionApiCallFlowTest, CreateApiCallBody) {
  ASSERT_TRUE(flow_->CreateApiCallBody().empty());
}

TEST_F(BoxAbortUploadSessionApiCallFlowTest, GetRequestTypeForBody) {
  ASSERT_EQ(flow_->GetRequestTypeForBody(std::string()), "DELETE");
}

TEST_F(BoxAbortUploadSessionApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(204));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(200));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(412));
}

TEST_F(BoxAbortUploadSessionApiCallFlowTest, ProcessApiCallSuccess) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_NO_CONTENT);
  flow_->ProcessApiCallSuccess(http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_NO_CONTENT);
}

TEST_F(BoxAbortUploadSessionApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CONFLICT);
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CONFLICT);
}

////////////////////////////////////////////////////////////////////////////////
// CommitUploadSession
////////////////////////////////////////////////////////////////////////////////

class BoxCommitUploadSessionApiCallFlowForTest
    : public BoxCommitUploadSessionApiCallFlow {
 public:
  using BoxCommitUploadSessionApiCallFlow::BoxCommitUploadSessionApiCallFlow;
  using BoxCommitUploadSessionApiCallFlow::CreateApiCallBody;
  using BoxCommitUploadSessionApiCallFlow::CreateApiCallHeaders;
  using BoxCommitUploadSessionApiCallFlow::CreateApiCallUrl;
  using BoxCommitUploadSessionApiCallFlow::IsExpectedSuccessCode;
  using BoxCommitUploadSessionApiCallFlow::ProcessApiCallFailure;
  using BoxCommitUploadSessionApiCallFlow::ProcessApiCallSuccess;
};

class BoxCommitUploadSessionApiCallFlowTest
    : public BoxApiCallFlowTest<BoxCommitUploadSessionApiCallFlowForTest> {
 protected:
  BoxCommitUploadSessionApiCallFlowTest()
      : upload_session_parts_(base::Value::Type::DICTIONARY) {
    base::Value part1(base::Value::Type::DICTIONARY);
    part1.SetStringKey("part_id", "BFDF5379");
    part1.SetIntKey("offset", 0);
    part1.SetIntKey("size", 8388608);
    part1.SetStringKey("sha1", "134b65991ed521fcfe4724b7d814ab8ded5185dc");

    base::Value part2(base::Value::Type::DICTIONARY);
    part2.SetStringKey("part_id", "E8A3ED8E");
    part2.SetIntKey("offset", 8388608);
    part2.SetIntKey("size", 1611392);
    part2.SetStringKey("sha1", "234b65934ed521fcfe3424b7d814ab8ded5185dc");

    base::Value parts(base::Value::Type::LIST);
    parts.Append(std::move(part1));
    parts.Append(std::move(part2));

    upload_session_parts_.SetKey("parts", std::move(parts));
    base::JSONWriter::Write(upload_session_parts_, &expected_body_);
  }

  void SetUp() override {
    flow_ = std::make_unique<BoxCommitUploadSessionApiCallFlowForTest>(
        base::BindOnce(&BoxCommitUploadSessionApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        kFileSystemBoxChunkedUploadCommitUrl, upload_session_parts_,
        kFileSystemBoxChunkedUploadSha);
  }

  void OnResponse(bool success,
                  int response_code,
                  base::TimeDelta retry_after) {
    processed_success_ = success;
    response_code_ = response_code;
    retry_after_ = retry_after;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  base::Value upload_session_parts_;
  std::string expected_body_;
  base::TimeDelta retry_after_;

  base::test::TaskEnvironment task_environment_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<BoxCommitUploadSessionApiCallFlowTest> factory_{this};
};

TEST_F(BoxCommitUploadSessionApiCallFlowTest, CreateApiCallUrl) {
  ASSERT_EQ(flow_->CreateApiCallUrl(), kFileSystemBoxChunkedUploadCommitUrl);
}

TEST_F(BoxCommitUploadSessionApiCallFlowTest, CreateApiCallHeaders) {
  net::HttpRequestHeaders headers = flow_->CreateApiCallHeaders();
  std::string digest;
  headers.GetHeader("digest", &digest);
  ASSERT_EQ(digest, kFileSystemBoxChunkedUploadSha);
}

TEST_F(BoxCommitUploadSessionApiCallFlowTest, CreateApiCallBody) {
  std::string body = flow_->CreateApiCallBody();
  ASSERT_EQ(body, expected_body_);
}

TEST_F(BoxCommitUploadSessionApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(201));
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(202));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(412));
}

TEST_F(BoxCommitUploadSessionApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CONFLICT);
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CONFLICT);
  ASSERT_EQ(retry_after_, base::TimeDelta());
}

TEST_F(BoxCommitUploadSessionApiCallFlowTest, ProcessApiCallSuccess_Retry) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_ACCEPTED);
  http_head->headers->AddHeader("Retry-After", "120");
  flow_->ProcessApiCallSuccess(http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_ACCEPTED);
  ASSERT_EQ(retry_after_, base::TimeDelta::FromSeconds(120));
}

TEST_F(BoxCommitUploadSessionApiCallFlowTest, ProcessApiCallSuccess_Created) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);
  flow_->ProcessApiCallSuccess(http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_EQ(retry_after_, base::TimeDelta::FromSeconds(0));
}

}  // namespace enterprise_connectors
