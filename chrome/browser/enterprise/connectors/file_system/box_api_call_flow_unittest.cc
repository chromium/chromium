// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for all subclasses of BoxApiCallFlow.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_response.h"

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
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

  void OnGenericResponse(BoxApiCallResponse response) {
    processed_success_ = response.success;
    response_code_ = response.net_or_http_code;
    box_error_code_ = response.box_error_code;
    box_request_id_ = response.box_request_id;
  }

  bool processed_success_ = false;
  int response_code_ = -1;
  std::string box_error_code_;
  std::string box_request_id_;

 private:
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder decoder_;
};

template <typename ApiCallMiniClass>
class BoxFolderApiCallFlowTest : public BoxApiCallFlowTest<ApiCallMiniClass> {
 protected:
  void OnResponse(BoxApiCallResponse response, const std::string& folder_id) {
    BoxApiCallFlowTest<ApiCallMiniClass>::OnGenericResponse(response);
    processed_folder_id_ = folder_id;
  }

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
  auto http_head = network::CreateURLResponseHead(net::HTTP_TOO_MANY_REQUESTS);
  std::unique_ptr<std::string> body =
      std::make_unique<std::string>(CreateFailureResponse(
          net::HTTP_TOO_MANY_REQUESTS, "rate_limit_exceeded"));
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_TOO_MANY_REQUESTS);
  ASSERT_EQ(box_error_code_, "rate_limit_exceeded");
  ASSERT_EQ(box_request_id_, kFileSystemBoxClientErrorResponseRequestId);
  ASSERT_EQ(processed_folder_id_, "");
}

TEST_F(BoxFindUpstreamFolderApiCallFlowTest, ProcessNetworkFailure) {
  flow_->ProcessApiCallFailure(net::ERR_TIMED_OUT, {}, {});
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::ERR_TIMED_OUT);
  ASSERT_EQ(processed_folder_id_, "");
}

struct FindUpstreamFolderSuccessResponses {
  base::StringPiece body;
  bool expected_success;
  std::string expected_folder_id;
};

class BoxFindUpstreamFolder_ProcessApiCallSuccessTest
    : public BoxFindUpstreamFolderApiCallFlowTest,
      public testing::WithParamInterface<FindUpstreamFolderSuccessResponses> {
 protected:
  void ProcessApiCallSuccess(base::StringPiece body) {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    flow_->ProcessApiCallSuccess(
        head.get(), std::make_unique<std::string>(body.data(), body.size()));
    base::RunLoop().RunUntilIdle();
  }
};

TEST_P(BoxFindUpstreamFolder_ProcessApiCallSuccessTest, ExtractFolderId) {
  const auto& body = GetParam().body;
  ProcessApiCallSuccess(body);
  ASSERT_EQ(response_code_, net::HTTP_OK);
  ASSERT_EQ(processed_success_, GetParam().expected_success) << body;
  ASSERT_EQ(processed_folder_id_, GetParam().expected_folder_id) << body;
}

const char kFileSystemBoxFindFolderResponseNoFolderId[] = R"({
  "entries": [
    {
      "etag": 1,
      "type": "folder",
      "sequence_id": 3,
      "name": "ChromeDownloads"
    }
  ]
})";

const char kFileSystemBoxFindFolderResponseEmptyFolderId[] = R"({
  "entries": [
    {
      "id": ,
      "etag": 1,
      "type": "folder",
      "sequence_id": 3,
      "name": "ChromeDownloads"
    }
  ]
})";

const char kFileSystemBoxFindFolderResponseBodyWithStringFolderId[] = R"({
  "entries": [
    {
      "id": "12345",
      "etag": 1,
      "type": "folder",
      "sequence_id": 3,
      "name": "ChromeDownloads"
    }
  ]
})";

const char kFileSystemBoxFindFolderResponseBodyWithFloatFolderId[] = R"({
  "entries": [
    {
      "id": 123.5,
      "etag": 1,
      "type": "folder",
      "sequence_id": 3,
      "name": "ChromeDownloads"
    }
  ]
})";

std::vector<FindUpstreamFolderSuccessResponses> kTestSuccessResponses = {
    {{}, false, ""},  // No body.
    {kEmptyResponseBody, false, ""},
    {"adgafdga", false, ""},  // Invalid body.
    {kFileSystemBoxFindFolderResponseEmptyEntriesList, true, ""},
    {kFileSystemBoxFindFolderResponseNoFolderId, false, ""},
    {kFileSystemBoxFindFolderResponseEmptyFolderId, false, ""},
    {kFileSystemBoxFindFolderResponseBody, true,
     kFileSystemBoxFindFolderResponseFolderId},
    {kFileSystemBoxFindFolderResponseBodyWithStringFolderId, true,
     kFileSystemBoxFindFolderResponseFolderId},
    {kFileSystemBoxFindFolderResponseBodyWithFloatFolderId, false, ""}};

INSTANTIATE_TEST_SUITE_P(BoxFindUpstreamFolderApiCallFlowTest,
                         BoxFindUpstreamFolder_ProcessApiCallSuccessTest,
                         testing::ValuesIn(kTestSuccessResponses));

////////////////////////////////////////////////////////////////////////////////
// GetFileFolder
////////////////////////////////////////////////////////////////////////////////

class BoxGetFileFolderApiCallFlowForTest : public BoxGetFileFolderApiCallFlow {
 public:
  using BoxGetFileFolderApiCallFlow::BoxGetFileFolderApiCallFlow;
  using BoxGetFileFolderApiCallFlow::CreateApiCallUrl;
  using BoxGetFileFolderApiCallFlow::ProcessApiCallFailure;
  using BoxGetFileFolderApiCallFlow::ProcessApiCallSuccess;
};

class BoxGetFileFolderApiCallFlowTest
    : public BoxFolderApiCallFlowTest<BoxGetFileFolderApiCallFlowForTest> {
 protected:
  void SetUp() override {
    flow_ = std::make_unique<BoxGetFileFolderApiCallFlowForTest>(
        base::BindOnce(&BoxGetFileFolderApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        kFileSystemBoxGetFileFolderFileId);
  }

  base::WeakPtrFactory<BoxGetFileFolderApiCallFlowTest> factory_{this};
};

TEST_F(BoxGetFileFolderApiCallFlowTest, CreateApiCallUrl) {
  GURL path(base::StrCat({kFileSystemBoxGetFileFolderUrl, "/",
                          kFileSystemBoxGetFileFolderFileId}));
  ASSERT_EQ(flow_->CreateApiCallUrl(), path);
}

TEST_F(BoxGetFileFolderApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_TOO_MANY_REQUESTS);
  std::unique_ptr<std::string> body =
      std::make_unique<std::string>(CreateFailureResponse(
          net::HTTP_TOO_MANY_REQUESTS, "rate_limit_exceeded"));
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_TOO_MANY_REQUESTS);
  ASSERT_EQ(box_error_code_, "rate_limit_exceeded");
  ASSERT_EQ(box_request_id_, kFileSystemBoxClientErrorResponseRequestId);
  ASSERT_EQ(processed_folder_id_, "");
}

class BoxGetFileFolderApiCallFlowTest_ProcessApiCallSuccess
    : public BoxGetFileFolderApiCallFlowTest {
 public:
  BoxGetFileFolderApiCallFlowTest_ProcessApiCallSuccess()
      : head_(network::CreateURLResponseHead(net::HTTP_OK)) {}

 protected:
  network::mojom::URLResponseHeadPtr head_;
};

TEST_F(BoxGetFileFolderApiCallFlowTest_ProcessApiCallSuccess, Normal) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_OK);
  flow_->ProcessApiCallSuccess(
      http_head.get(),
      std::make_unique<std::string>(kFileSystemBoxGetFileFolderResponseBody));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
  ASSERT_EQ(processed_folder_id_, kFileSystemBoxGetFileFolderResponseFolderId);
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
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(409));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(403));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
}

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_TOO_MANY_REQUESTS);
  std::unique_ptr<std::string> body =
      std::make_unique<std::string>(CreateFailureResponse(
          net::HTTP_TOO_MANY_REQUESTS, "rate_limit_exceeded"));
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_TOO_MANY_REQUESTS);
  ASSERT_EQ(box_error_code_, "rate_limit_exceeded");
  ASSERT_EQ(box_request_id_, kFileSystemBoxClientErrorResponseRequestId);
  ASSERT_EQ(processed_folder_id_, "");
}

class BoxCreateUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess
    : public BoxCreateUpstreamFolderApiCallFlowTest {
 public:
  BoxCreateUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess()
      : head_(network::CreateURLResponseHead(net::HTTP_CREATED)) {}

 protected:
  network::mojom::URLResponseHeadPtr head_;
};

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess, Created) {
  flow_->ProcessApiCallSuccess(
      head_.get(),
      std::make_unique<std::string>(kFileSystemBoxCreateFolderResponseBody));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_EQ(processed_folder_id_, kFileSystemBoxCreateFolderResponseFolderId);
}

TEST_F(BoxCreateUpstreamFolderApiCallFlowTest_ProcessApiCallSuccess, Conflict) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CONFLICT);
  flow_->ProcessApiCallSuccess(
      http_head.get(), std::make_unique<std::string>(
                           kFileSystemBoxCreateFolderConflictResponseBody));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CONFLICT);
  ASSERT_EQ(processed_folder_id_, kFileSystemBoxCreateFolderResponseFolderId);
}

////////////////////////////////////////////////////////////////////////////////
// PreflightCheck
////////////////////////////////////////////////////////////////////////////////

class BoxPreflightCheckApiCallFlowForTest
    : public BoxPreflightCheckApiCallFlow {
 public:
  using BoxPreflightCheckApiCallFlow::BoxPreflightCheckApiCallFlow;
  using BoxPreflightCheckApiCallFlow::CreateApiCallBody;
  using BoxPreflightCheckApiCallFlow::CreateApiCallBodyContentType;
  using BoxPreflightCheckApiCallFlow::CreateApiCallUrl;
  using BoxPreflightCheckApiCallFlow::IsExpectedSuccessCode;
  using BoxPreflightCheckApiCallFlow::ProcessApiCallFailure;
  using BoxPreflightCheckApiCallFlow::ProcessApiCallSuccess;
};

class BoxPreflightCheckApiCallFlowTest
    : public BoxApiCallFlowTest<BoxPreflightCheckApiCallFlowForTest> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append(file_name_);

    flow_ = std::make_unique<BoxPreflightCheckApiCallFlowForTest>(
        base::BindOnce(&BoxPreflightCheckApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        file_name_, folder_id_);
  }

  void OnResponse(BoxApiCallResponse response) {
    OnGenericResponse(response);
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  const std::string folder_id_{"1337"};
  const base::FilePath file_name_{
      FILE_PATH_LITERAL("box_preflight_check_test.txt")};
  base::FilePath file_path_;

  base::ScopedTempDir temp_dir_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<BoxPreflightCheckApiCallFlowTest> factory_{this};
};

TEST_F(BoxPreflightCheckApiCallFlowTest, CreateApiCallUrl) {
  GURL url(kFileSystemBoxPreflightCheckUrl);
  ASSERT_EQ(flow_->CreateApiCallUrl(), url);
}

TEST_F(BoxPreflightCheckApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(200));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(403));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
}

TEST_F(BoxPreflightCheckApiCallFlowTest, ProcessApiCallSuccess) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_OK);
  flow_->ProcessApiCallSuccess(http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_OK);
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
  using BoxWholeFileUploadApiCallFlow::SetFileReadForTesting;
};

class BoxWholeFileUploadApiCallFlowTest
    : public BoxApiCallFlowTest<BoxWholeFileUploadApiCallFlowForTest> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append(file_name_);

    flow_ = std::make_unique<BoxWholeFileUploadApiCallFlowForTest>(
        base::BindOnce(&BoxWholeFileUploadApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        folder_id_, mime_type_, file_name_, file_path_);
  }

  void OnResponse(BoxApiCallResponse response, const std::string& file_id) {
    OnGenericResponse(response);
    file_id_ = file_id;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  std::string MakeExpectedBody() {
    // Body format for multipart/form-data reference:
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Type
    // Request body fields reference:
    // https://developer.box.com/reference/post-files-content/
    std::string content_type = flow_->CreateApiCallBodyContentType();
    std::string expected_type("multipart/form-data; boundary=");

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
    expected_body +=
        "Content-Disposition: form-data; name=\"file\"; filename=\"";
    expected_body +=
        file_name_.AsUTF8Unsafe() +  // AsUTF8Unsafe() to compile on Windows
        "\"\r\nContent-Type: " + mime_type_ + "\r\n\r\n";
    expected_body += file_content_ + "\r\n";
    expected_body += multipart_boundary + "--\r\n";
    return expected_body;
  }

  base::FilePath file_path_;
  const std::string folder_id_{"13579"};
  const std::string mime_type_{"text/plain"};
  const base::FilePath file_name_{
      FILE_PATH_LITERAL("box_whole_file_upload_test.txt")};
  const std::string file_content_{"<TestContent>~~~123456789~~~</TestContent>"};

  std::string file_id_;

  base::ScopedTempDir temp_dir_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<BoxWholeFileUploadApiCallFlowTest> factory_{this};
};

TEST_F(BoxWholeFileUploadApiCallFlowTest, CreateApiCallUrl) {
  GURL url(kFileSystemBoxDirectUploadUrl);
  ASSERT_EQ(flow_->CreateApiCallUrl(), url);
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, CreateApiCallBodyAndContentType) {
  std::string content_type = flow_->CreateApiCallBodyContentType();
  std::string expected_type("multipart/form-data; boundary=");
  ASSERT_EQ(content_type.substr(0, expected_type.size()), expected_type);

  flow_->SetFileReadForTesting(file_content_);
  ASSERT_EQ(flow_->CreateApiCallBody(), MakeExpectedBody());
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(201));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(403));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, ProcessApiCallSuccess_EmptyBody) {
  // Create a temporary file to be deleted in ProcessApiCallSuccess().
  ASSERT_TRUE(base::WriteFile(file_path_, "BoxWholeFileUploadApiCallFlowTest"))
      << file_path_;

  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>());
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_TRUE(processed_success_) << "Failed with file " << file_path_;
  ASSERT_TRUE(file_id_.empty());
  ASSERT_EQ(BoxApiCallFlow::MakeUrlToShowFile(file_id_), GURL());
  ASSERT_TRUE(base::PathExists(file_path_))
      << "File " << file_path_
      << " must still exist / not have been deleted by another thread so that "
         "BoxUploader can delete it.";
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, ProcessApiCallSuccess_ValidUrl) {
  // Create a temporary file to be deleted in ProcessApiCallSuccess().
  ASSERT_TRUE(base::WriteFile(file_path_, "BoxWholeFileUploadApiCallFlowTest"))
      << file_path_;

  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);
  std::string body(kFileSystemBoxUploadResponseBody);

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_TRUE(processed_success_) << "Failed with file " << file_path_;
  ASSERT_FALSE(file_id_.empty());
  ASSERT_EQ(BoxApiCallFlow::MakeUrlToShowFile(file_id_),
            GURL(kFileSystemBoxUploadResponseFileUrl));
  ASSERT_TRUE(base::PathExists(file_path_))
      << "File " << file_path_
      << " must still exist / not have been deleted by another thread so that "
         "BoxUploader can delete it.";
}

TEST_F(BoxWholeFileUploadApiCallFlowTest,
       ProcessApiCallSuccess_NoFileToDelete) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);
  ASSERT_FALSE(base::PathExists(file_path_));  // Make sure file doesn't exist.

  // Because we post tasks to base::ThreadPool, cannot use
  // base::RunLoop().RunUntilIdle().
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>());
  // Empty placeholder body since we don't read from body for now.
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_TRUE(processed_success_) << "API call flow success should not depend "
                                     "on whether file exists to be deleted.";
  ASSERT_FALSE(base::PathExists(file_path_));  // Make sure no file was created.
}

TEST_F(BoxWholeFileUploadApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CONFLICT);
  std::unique_ptr<std::string> body = std::make_unique<std::string>(
      CreateFailureResponse(net::HTTP_CONFLICT, "storage_limit_exceeded"));
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  run_loop.Run();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CONFLICT);
  ASSERT_EQ(box_error_code_, "storage_limit_exceeded");
  ASSERT_EQ(box_request_id_, kFileSystemBoxClientErrorResponseRequestId);
}

class BoxWholeFileUploadApiCallFlowFileReadTest
    : public BoxWholeFileUploadApiCallFlowTest {
 public:
  BoxWholeFileUploadApiCallFlowFileReadTest()
      : url_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    test_url_loader_factory_.SetInterceptor(base::BindRepeating(
        &BoxWholeFileUploadApiCallFlowFileReadTest::VerifyRequest,
        base::Unretained(this)));
  }

 protected:
  void VerifyRequest(const network::ResourceRequest& request) {
    ASSERT_EQ(request.url, kFileSystemBoxDirectUploadUrl);
    // Check that file was read and formatted into request body string properly,
    // without going down the rabbit hole of request.request_body->elements()->
    // front().As<network::DataElementBytes>().AsStringPiece().
    ASSERT_EQ(flow_->CreateApiCallBody(), MakeExpectedBody());
    ++request_sent_count_;
  }

  size_t request_sent_count_ = 0;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_factory_;
};

TEST_F(BoxWholeFileUploadApiCallFlowFileReadTest, GoodUpload) {
  ASSERT_TRUE(base::WriteFile(file_path_, file_content_)) << file_path_;

  test_url_loader_factory_.AddResponse(kFileSystemBoxDirectUploadUrl,
                                       std::string(), net::HTTP_CREATED);
  // Empty placeholder body since we don't read from body for now.

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->Start(url_factory_, "test_token");
  run_loop.Run();

  ASSERT_EQ(request_sent_count_, 1U);
  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_TRUE(processed_success_) << "Failed with file " << file_path_;
  ASSERT_TRUE(base::PathExists(file_path_))
      << "File " << file_path_
      << " must still exist / not have been deleted by another thread so that "
         "BoxUploader can delete it.";
}

TEST_F(BoxWholeFileUploadApiCallFlowFileReadTest, NoFile) {
  ASSERT_FALSE(base::PathExists(file_path_));

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->Start(url_factory_, "test_token");
  run_loop.Run();

  // Because file read already failed before any actual API calls are made,
  // there should be no API calls made, and therefore no HTTP response code.
  ASSERT_EQ(request_sent_count_, 0U);
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

  void OnResponse(BoxApiCallResponse response,
                  base::Value session_endpoints,
                  size_t part_size) {
    OnGenericResponse(response);
    if (response.success) {
      ASSERT_TRUE(session_endpoints.is_dict());
      session_upload_endpoint_ =
          session_endpoints.FindPath("upload_part")->GetString();
      session_abort_endpoint_ =
          session_endpoints.FindPath("abort")->GetString();
      session_commit_endpoint_ =
          session_endpoints.FindPath("commit")->GetString();
      part_size_ = part_size;
    }
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  std::string session_upload_endpoint_;
  std::string session_abort_endpoint_;
  std::string session_commit_endpoint_;
  size_t part_size_ = 0;

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
      http_head.get(),
      std::make_unique<std::string>(
          kFileSystemBoxChunkedUploadCreateSessionResponseBody));
  run_loop.Run();

  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_TRUE(processed_success_);
  EXPECT_EQ(session_upload_endpoint_, kFileSystemBoxChunkedUploadSessionUrl);
  EXPECT_EQ(session_abort_endpoint_, kFileSystemBoxChunkedUploadSessionUrl);
  EXPECT_EQ(session_commit_endpoint_, kFileSystemBoxChunkedUploadCommitUrl);
  EXPECT_EQ(part_size_,
            kFileSystemBoxChunkedUploadCreateSessionResponsePartSize);
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
  auto http_head = network::CreateURLResponseHead(net::HTTP_BAD_REQUEST);
  std::unique_ptr<std::string> body = std::make_unique<std::string>(
      CreateFailureResponse(net::HTTP_BAD_REQUEST, "item_name_invalid"));

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  run_loop.Run();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_BAD_REQUEST);
  ASSERT_EQ(box_error_code_, "item_name_invalid");
  ASSERT_EQ(box_request_id_, kFileSystemBoxClientErrorResponseRequestId);
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

  void OnResponse(BoxApiCallResponse response, base::Value) {
    OnGenericResponse(response);
  }

  const std::string file_content_;
  const std::string expected_sha_;
  const std::string expected_content_range_;

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
  std::unique_ptr<std::string> body = std::make_unique<std::string>(
      CreateFailureResponse(net::HTTP_CONFLICT, "name_in_use"));
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CONFLICT);
  ASSERT_EQ(box_error_code_, "name_in_use");
  ASSERT_EQ(box_request_id_, kFileSystemBoxClientErrorResponseRequestId);
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
        base::BindOnce(&BoxAbortUploadSessionApiCallFlowTest::OnGenericResponse,
                       factory_.GetWeakPtr()),
        kFileSystemBoxChunkedUploadSessionUrl);
  }

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
  std::unique_ptr<std::string> body = std::make_unique<std::string>(
      CreateFailureResponse(net::HTTP_CONFLICT, "operation_blocked_temporary"));
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CONFLICT);
  ASSERT_EQ(box_error_code_, "operation_blocked_temporary");
  ASSERT_EQ(box_request_id_, kFileSystemBoxClientErrorResponseRequestId);
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
      : upload_session_parts_(base::Value::Type::LIST) {
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

    upload_session_parts_.Append(std::move(part1));
    upload_session_parts_.Append(std::move(part2));

    base::Value parts_body(base::Value::Type::DICTIONARY);
    parts_body.SetKey("parts", upload_session_parts_.Clone());
    // The request body should be in the form of "parts": [list of parts], but
    // only the list is passed into the class.

    base::JSONWriter::Write(parts_body, &expected_body_);
  }

  void SetUp() override {
    flow_ = std::make_unique<BoxCommitUploadSessionApiCallFlowForTest>(
        base::BindOnce(&BoxCommitUploadSessionApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()),
        kFileSystemBoxChunkedUploadCommitUrl, upload_session_parts_,
        kFileSystemBoxChunkedUploadSha);
  }

  void OnResponse(BoxApiCallResponse response,
                  base::TimeDelta retry_after,
                  const std::string& file_id) {
    OnGenericResponse(response);
    retry_after_ = retry_after;
    file_id_ = file_id;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  base::Value upload_session_parts_;
  std::string expected_body_;
  base::TimeDelta retry_after_;
  std::string file_id_;

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
  ASSERT_EQ(digest,
            BoxApiCallFlow::FormatSHA1Digest(kFileSystemBoxChunkedUploadSha));
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
  auto http_head = network::CreateURLResponseHead(net::HTTP_GONE);
  std::unique_ptr<std::string> body = std::make_unique<std::string>(
      CreateFailureResponse(net::HTTP_GONE, "session_expired"));
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_GONE);
  ASSERT_EQ(box_error_code_, "session_expired");
  ASSERT_EQ(box_request_id_, kFileSystemBoxClientErrorResponseRequestId);
  ASSERT_EQ(retry_after_, base::TimeDelta());
}

TEST_F(BoxCommitUploadSessionApiCallFlowTest, ProcessApiCallSuccess_Retry) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_ACCEPTED);
  http_head->headers->AddHeader("Retry-After", "120");
  flow_->ProcessApiCallSuccess(http_head.get(), {});
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_ACCEPTED);
  ASSERT_EQ(retry_after_, base::Seconds(120));
}

TEST_F(BoxCommitUploadSessionApiCallFlowTest, ProcessApiCallSuccess_Created) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_CREATED);
  std::string body(kFileSystemBoxUploadResponseBody);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  run_loop.Run();

  ASSERT_TRUE(processed_success_);
  ASSERT_EQ(response_code_, net::HTTP_CREATED);
  ASSERT_EQ(retry_after_, base::Seconds(0));
  ASSERT_FALSE(file_id_.empty());
  ASSERT_EQ(BoxApiCallFlow::MakeUrlToShowFile(file_id_),
            GURL(kFileSystemBoxUploadResponseFileUrl));
}

////////////////////////////////////////////////////////////////////////////////
// GetCurrentUser
////////////////////////////////////////////////////////////////////////////////

class BoxGetCurrentUserApiCallFlowForTest
    : public BoxGetCurrentUserApiCallFlow {
 public:
  using BoxGetCurrentUserApiCallFlow::BoxGetCurrentUserApiCallFlow;
  using BoxGetCurrentUserApiCallFlow::CreateApiCallBody;
  using BoxGetCurrentUserApiCallFlow::CreateApiCallBodyContentType;
  using BoxGetCurrentUserApiCallFlow::CreateApiCallUrl;
  using BoxGetCurrentUserApiCallFlow::IsExpectedSuccessCode;
  using BoxGetCurrentUserApiCallFlow::ProcessApiCallSuccess;
  using BoxGetCurrentUserApiCallFlow::ProcessFailure;
};

class BoxGetCurrentUserApiCallFlowTest
    : public BoxApiCallFlowTest<BoxGetCurrentUserApiCallFlowForTest> {
 protected:
  void SetUp() override {
    flow_ = std::make_unique<BoxGetCurrentUserApiCallFlowForTest>(
        base::BindOnce(&BoxGetCurrentUserApiCallFlowTest::OnResponse,
                       factory_.GetWeakPtr()));
  }

  void OnResponse(BoxApiCallResponse response, base::Value json) {
    OnGenericResponse(response);
    if (json.FindStringPath("enterprise.id")) {
      enterprise_id_ =
          std::make_unique<std::string>(*json.FindStringPath("enterprise.id"));
    }
  }

  std::unique_ptr<std::string> enterprise_id_;
  base::OnceClosure quit_closure_;
  base::WeakPtrFactory<BoxGetCurrentUserApiCallFlowTest> factory_{this};
};

TEST_F(BoxGetCurrentUserApiCallFlowTest, CreateApiCallUrl) {
  GURL url(kFileSystemBoxGetUserUrl);
  ASSERT_EQ(flow_->CreateApiCallUrl(), url);
}

TEST_F(BoxGetCurrentUserApiCallFlowTest, IsExpectedSuccessCode) {
  ASSERT_TRUE(flow_->IsExpectedSuccessCode(200));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(400));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(403));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(404));
  ASSERT_FALSE(flow_->IsExpectedSuccessCode(409));
}

TEST_F(BoxGetCurrentUserApiCallFlowTest, ProcessApiCallSuccess) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_OK);
  std::string body(R"({
    "type": "user",
    "id": "9876",
    "login": "wile.e.coyote@acme.com",
    "name": "Wile E. Coyote",
    "enterprise": {
      "type": "enterprise",
      "id": "31415926",
      "name": "MegaCorp"
    }
  })");
  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(processed_success_);
  ASSERT_TRUE(enterprise_id_ != nullptr);
  ASSERT_EQ(*enterprise_id_, "31415926");
  ASSERT_EQ(response_code_, net::HTTP_OK);
}

TEST_F(BoxGetCurrentUserApiCallFlowTest,
       ProcessApiCallSuccess_NonEnterpriseUser) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_OK);
  std::string body(R"({
    "type": "user",
    "id": "1234",
    "enterprise": null
  })");
  flow_->ProcessApiCallSuccess(http_head.get(),
                               std::make_unique<std::string>(body));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(enterprise_id_, nullptr);
  ASSERT_EQ(response_code_, net::HTTP_OK);
}

TEST_F(BoxGetCurrentUserApiCallFlowTest, ProcessApiCallFailure) {
  auto http_head = network::CreateURLResponseHead(net::HTTP_TOO_MANY_REQUESTS);
  std::unique_ptr<std::string> body =
      std::make_unique<std::string>(CreateFailureResponse(
          net::HTTP_TOO_MANY_REQUESTS, "rate_limit_exceeded"));
  flow_->ProcessApiCallFailure(net::OK, http_head.get(), std::move(body));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(processed_success_);
  ASSERT_EQ(enterprise_id_, nullptr);
  ASSERT_EQ(response_code_, net::HTTP_TOO_MANY_REQUESTS);
  ASSERT_EQ(box_error_code_, "rate_limit_exceeded");
}

// base::Value(base::Value::Type::DICTIONARY);
}  // namespace enterprise_connectors
