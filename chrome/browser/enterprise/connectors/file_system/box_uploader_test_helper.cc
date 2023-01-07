// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_uploader_test_helper.h"

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
std::string GetTestName() {
  // Gets information about the currently running test.
  // Do NOT delete the returned object - it's managed by the UnitTest class.
  const testing::TestInfo* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  std::string name(test_info->test_suite_name());
  name += ".";
  name += test_info->name();
  return name;
}
}  // namespace

namespace enterprise_connectors {

////////////////////////////////////////////////////////////////////////////////
// BoxUploaderTestBase
////////////////////////////////////////////////////////////////////////////////

BoxUploaderTestBase::BoxUploaderTestBase(
    base::FilePath::StringPieceType file_name)
    : test_item_(file_name),
      url_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {
  test_item_.SetMimeType("text/plain");
  EXPECT_TRUE(profile_manager_.SetUp());
  prefs_ = profile_manager_.CreateTestingProfile("test-user")->GetPrefs();
  SetInterceptorForURLLoader(
      base::BindRepeating(&BoxUploaderTestBase::SetNextResponseForURLLoader,
                          base::Unretained(this)));
}

BoxUploaderTestBase::~BoxUploaderTestBase() = default;

base::FilePath BoxUploaderTestBase::GetFilePath() const {
  return test_item_.GetFullPath();
}

void BoxUploaderTestBase::CreateTemporaryFile() {
  CreateTemporaryFileWithContent(GetTestName());
}

void BoxUploaderTestBase::CreateTemporaryFileWithContent(std::string content) {
  auto path = test_item_.GetFullPath();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(base::WriteFile(path, content)) << "Failed to create " << path;
  test_item_.SetTotalBytes(content.size());
  ASSERT_EQ(test_item_.GetTotalBytes(), static_cast<int64_t>(content.size()));
}

void BoxUploaderTestBase::InitUploader(BoxUploader* uploader) {
  ASSERT_TRUE(uploader);
  ASSERT_TRUE(prefs_);
  uploader->Init(base::BindRepeating(&BoxUploaderTestBase::AuthenticationRetry,
                                     base::Unretained(this)),
                 base::BindRepeating(&BoxUploaderTestBase::OnProgressUpdate,
                                     base::Unretained(this)),
                 base::BindOnce(&BoxUploaderTestBase::OnUploaderFinished,
                                base::Unretained(this)),
                 prefs_);
}

void BoxUploaderTestBase::InitFolderIdInPrefs(std::string folder_id) {
  ASSERT_TRUE(prefs_);
  SetDefaultFolder(prefs_, kFileSystemServiceProviderPrefNameBox, folder_id,
                   "ChromeDownloads");
}

void BoxUploaderTestBase::SetInterceptorForURLLoader(
    network::TestURLLoaderFactory::Interceptor interceptor) {
  test_url_loader_factory_.SetInterceptor(interceptor);
}

void BoxUploaderTestBase::AddFetchResult(const std::string& url,
                                         net::HttpStatusCode code,
                                         std::string body) {
  ASSERT_EQ(sequential_responses_.count(GURL(url)), 0U)
      << "Already has sequential response(s) for " << url;
  if (code >= 400 && body.empty()) {
    body = CreateFailureResponse(code, "some_box_error_code");
  }
  test_url_loader_factory_.AddResponse(url, std::move(body), code);
  repeating_responses_.emplace(GURL(url));
}

void BoxUploaderTestBase::AddSequentialFetchResult(
    const std::string& url,
    network::mojom::URLResponseHeadPtr head,
    std::string body) {
  ASSERT_EQ(repeating_responses_.count(GURL(url)), 0U)
      << "Already has repeating response(s) for " << url;
  const auto code = head->headers->response_code();
  if (code >= 400 && body.empty()) {
    body = CreateFailureResponse(code, "some_box_error_code");
  }
  sequential_responses_.emplace(
      url,
      HttpResponse(idx_sequential_add++, std::move(head), std::move(body)));
}

void BoxUploaderTestBase::AddSequentialFetchResult(const std::string& url,
                                                   net::HttpStatusCode code,
                                                   std::string body) {
  auto head = network::CreateURLResponseHead(code);
  AddSequentialFetchResult(url, std::move(head), std::move(body));
}

void BoxUploaderTestBase::ClearFetchResults(const std::string& url) {
  GURL gurl(url);
  if (repeating_responses_.count(gurl)) {
    repeating_responses_.erase(gurl);
  } else if (sequential_responses_.count(gurl)) {
    auto iter = sequential_responses_.find(gurl);
    sequential_responses_.erase(iter);
  }
}

size_t BoxUploaderTestBase::GetPendingSequentialResponsesCount(
    const std::string& url) const {
  return sequential_responses_.count(GURL(url));
}

void BoxUploaderTestBase::InitQuitClosure() {
  run_loop_ = std::make_unique<base::RunLoop>();
  quit_closure_ = run_loop_->QuitClosure();
}

void BoxUploaderTestBase::RunWithQuitClosure() {
  run_loop_->Run();
}

void BoxUploaderTestBase::Quit() {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void BoxUploaderTestBase::AuthenticationRetry() {
  ++authentication_retry_;
  Quit();
}

void BoxUploaderTestBase::OnProgressUpdate(
    const download::DownloadItemRenameProgressUpdate& update) {
  ++progress_update_cb_called_;
  file_name_reported_back_ = update.target_file_name;
  reroute_info_reported_back_ = update.reroute_info;
}

void BoxUploaderTestBase::OnUploaderFinished(
    download::DownloadInterruptReason reason,
    const base::FilePath& final_name) {
  download_thread_cb_called_ = true;
  upload_success_ = (reason == download::DOWNLOAD_INTERRUPT_REASON_NONE);
  reason_ = reason;
  DLOG(INFO) << reason;
  if (upload_success_)
    file_name_reported_back_ = final_name;
  Quit();
}

void BoxUploaderTestBase::SetNextResponseForURLLoader(
    const network::ResourceRequest& request) {
  if (!sequential_responses_.count(request.url)) {
    ASSERT_TRUE(repeating_responses_.count(request.url)) << request.url;
    return;
  }
  auto iter = sequential_responses_.find(request.url);
  auto& response = iter->second;

  ASSERT_EQ(response.idx_, idx_sequential_fetch);
  test_url_loader_factory_.AddResponse(request.url, std::move(response.head_),
                                       response.body_,
                                       network::URLLoaderCompletionStatus());
  ++idx_sequential_fetch;
  sequential_responses_.erase(iter);
}

void BoxUploaderTestBase::TearDown() {
  ASSERT_EQ(sequential_responses_.size(), 0U);
  testing::Test::TearDown();
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploaderTestBase::HttpResponse
////////////////////////////////////////////////////////////////////////////////

BoxUploaderTestBase::HttpResponse::HttpResponse(
    size_t idx,
    network::mojom::URLResponseHeadPtr head,
    std::string body)
    : idx_(idx), head_(std::move(head)), body_(std::move(body)) {}

BoxUploaderTestBase::HttpResponse::~HttpResponse() = default;

BoxUploaderTestBase::HttpResponse::HttpResponse(HttpResponse&& response)
    : idx_(response.idx_),
      head_(std::move(response.head_)),
      body_(std::move(response.body_)) {}

}  // namespace enterprise_connectors
