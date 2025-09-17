// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_page_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom-data-view.h"
#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_delegate.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/receiver/receiver_handler_delegate.h"
#include "chromeos/ash/components/boca/receiver/register_receiver_request.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"
#include "chromeos/ash/components/boca/util.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace ash::boca_receiver {
namespace {

using ::testing::_;
using ::testing::NotNull;
using ::testing::Return;

constexpr std::string_view kReceiverId = "AB12";

class MockUntrustedPage : public mojom::UntrustedPage {
 public:
  MockUntrustedPage() = default;
  ~MockUntrustedPage() override = default;

  mojo::PendingRemote<mojom::UntrustedPage> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, OnInitReceiverInfo, (mojom::ReceiverInfoPtr), (override));

  MOCK_METHOD(void, OnInitReceiverError, (), (override));

  MOCK_METHOD(void,
              OnConnecting,
              (mojom::UserInfoPtr, mojom::UserInfoPtr),
              (override));

  MOCK_METHOD(void, OnFrameReceived, (const SkBitmap&), (override));

  MOCK_METHOD(void,
              OnConnectionClosed,
              (mojom::ConnectionClosedReason),
              (override));

 private:
  mojo::Receiver<mojom::UntrustedPage> receiver_{this};
};

class MockReceiverHandlerDelegate : public ReceiverHandlerDelegate {
 public:
  MockReceiverHandlerDelegate() = default;
  ~MockReceiverHandlerDelegate() override = default;

  MOCK_METHOD(std::unique_ptr<boca::InvalidationServiceImpl>,
              CreateInvalidationService,
              (boca::InvalidationServiceDelegate*),
              (const, override));

  MOCK_METHOD(std::unique_ptr<google_apis::RequestSender>,
              CreateRequestSender,
              (std::string_view, const net::NetworkTrafficAnnotationTag&),
              (const, override));

  MOCK_METHOD(std::unique_ptr<boca::SpotlightRemotingClientManager>,
              CreateRemotingClientManager,
              (),
              (override));

  MOCK_METHOD(bool, IsAppEnabled, (std::string_view), (override));
};

class BocaReceiverUntrustedPageHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    ON_CALL(handler_delegate_, CreateRequestSender)
        .WillByDefault(
            [this](std::string_view requester_id,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation) {
              return std::make_unique<google_apis::RequestSender>(
                  std::make_unique<google_apis::DummyAuthService>(),
                  url_loader_factory_.GetSafeWeakWrapper(),
                  task_environment_.GetMainThreadTaskRunner(),
                  "test-user-agent", traffic_annotation);
            });

    ON_CALL(handler_delegate_, CreateInvalidationService)
        .WillByDefault([this](boca::InvalidationServiceDelegate* delegate) {
          invalidation_service_delegate_ = delegate;
          return nullptr;
        });
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  MockReceiverHandlerDelegate handler_delegate_;
  MockUntrustedPage page_;
  raw_ptr<boca::InvalidationServiceDelegate> invalidation_service_delegate_;
};

TEST_F(BocaReceiverUntrustedPageHandlerTest, InitWhenAppDisabled) {
  base::test::TestFuture<void> signal;
  EXPECT_CALL(handler_delegate_, IsAppEnabled).WillOnce(Return(false));
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&signal]() {
    signal.GetCallback().Run();
  });
  EXPECT_CALL(handler_delegate_, CreateInvalidationService).Times(0);
  EXPECT_CALL(handler_delegate_, CreateRequestSender).Times(0);

  BocaReceiverUntrustedPageHandler handler(page_.BindAndGetRemote(),
                                           &handler_delegate_);

  EXPECT_TRUE(signal.Wait());
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, RegisterSuccess) {
  GURL url =
      GURL(boca::GetSchoolToolsUrl()).Resolve(RegisterReceiverRequest::kUrl);
  url_loader_factory_.AddResponse(url.spec(), R"({"receiver_id": "AB12"})");
  EXPECT_CALL(handler_delegate_, IsAppEnabled).WillOnce(Return(true));
  EXPECT_CALL(handler_delegate_, CreateInvalidationService).Times(1);
  EXPECT_CALL(handler_delegate_, CreateRequestSender).Times(1);
  base::test::TestFuture<mojom::ReceiverInfoPtr> on_init_receiver_info_future;
  EXPECT_CALL(page_, OnInitReceiverInfo)
      .WillOnce([&on_init_receiver_info_future](
                    mojom::ReceiverInfoPtr received_info) {
        on_init_receiver_info_future.GetCallback().Run(
            std::move(received_info));
      });

  BocaReceiverUntrustedPageHandler handler(page_.BindAndGetRemote(),
                                           &handler_delegate_);

  base::test::TestFuture<bool> token_upload_future;
  ASSERT_THAT(invalidation_service_delegate_, NotNull());
  invalidation_service_delegate_->UploadToken(
      "fcm-token", token_upload_future.GetCallback());

  mojom::ReceiverInfoPtr receiver_info = on_init_receiver_info_future.Take();
  EXPECT_TRUE(token_upload_future.Get());
  EXPECT_EQ(receiver_info->id, kReceiverId);
}

TEST_F(BocaReceiverUntrustedPageHandlerTest, RegisterFailure) {
  GURL url =
      GURL(boca::GetSchoolToolsUrl()).Resolve(RegisterReceiverRequest::kUrl);
  url_loader_factory_.AddResponse(
      url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::HTTP_FORBIDDEN));
  EXPECT_CALL(handler_delegate_, IsAppEnabled).WillOnce(Return(true));
  EXPECT_CALL(handler_delegate_, CreateInvalidationService).Times(1);
  EXPECT_CALL(handler_delegate_, CreateRequestSender).Times(1);
  base::test::TestFuture<void> signal;
  EXPECT_CALL(page_, OnInitReceiverError).WillOnce([&signal]() {
    signal.GetCallback().Run();
  });

  BocaReceiverUntrustedPageHandler handler(page_.BindAndGetRemote(),
                                           &handler_delegate_);

  base::test::TestFuture<bool> token_upload_future;
  ASSERT_THAT(invalidation_service_delegate_, NotNull());
  invalidation_service_delegate_->UploadToken(
      "fcm-token", token_upload_future.GetCallback());

  EXPECT_FALSE(token_upload_future.Get());
  EXPECT_TRUE(signal.Wait());
}

}  // namespace
}  // namespace ash::boca_receiver
