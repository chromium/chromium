// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"

#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_system_state.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/drive/service/drive_service_interface.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {
using ::testing::Return;
}  // namespace

FakeScannerProfileScopedDelegate::FakeScannerProfileScopedDelegate() {
  ON_CALL(*this, GetSystemState)
      .WillByDefault(Return(
          ScannerSystemState(ScannerStatus::kEnabled, /*failed_checks=*/{})));
}

FakeScannerProfileScopedDelegate::~FakeScannerProfileScopedDelegate() = default;

drive::DriveServiceInterface*
FakeScannerProfileScopedDelegate::GetDriveService() {
  return &drive_service_;
}

google_apis::RequestSender*
FakeScannerProfileScopedDelegate::GetGoogleApisRequestSender() {
  if (request_sender_ == nullptr) {
    request_sender_ = std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(),
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            /*network_service=*/nullptr,
            /*is_trusted=*/true),
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
        "test-user-agent", TRAFFIC_ANNOTATION_FOR_TESTS);

    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();
    test_server_->RegisterRequestHandler(
        base::BindRepeating(&FakeScannerProfileScopedDelegate::HandleRequest,
                            base::Unretained(this)));
    CHECK(test_server_->Start());

    gaia_urls_overrider_ = std::make_unique<GaiaUrlsOverriderForTesting>(
        base::CommandLine::ForCurrentProcess(), "people_api_origin_url",
        test_server_->base_url().spec());

    CHECK_EQ(test_server_->base_url(),
             GaiaUrls::GetInstance()->people_api_origin_url());
  }

  return request_sender_.get();
}

std::unique_ptr<net::test_server::HttpResponse>
FakeScannerProfileScopedDelegate::HandleRequest(
    const net::test_server::HttpRequest& request) {
  CHECK(!request_callback_.is_null());

  return request_callback_.Run(request);
}

bool FakeScannerProfileScopedDelegate::IsGoogler() {
  return false;
}

}  // namespace ash
