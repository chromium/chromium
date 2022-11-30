// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/pay/arc_payment_app_bridge.h"

#include <utility>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_payment_app_bridge_test_support.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {
namespace {

class ArcPaymentAppBridgeTest : public testing::Test {
 public:
  ArcPaymentAppBridgeTest() = default;
  ~ArcPaymentAppBridgeTest() override = default;

  ArcPaymentAppBridgeTest(const ArcPaymentAppBridgeTest& other) = delete;
  ArcPaymentAppBridgeTest& operator=(const ArcPaymentAppBridgeTest& other) =
      delete;

  void OnPaymentImplementedResponse(
      mojom::IsPaymentImplementedResultPtr response) {
    is_implemented_ = std::move(response);
  }

  void OnIsReadyToPayResponse(mojom::IsReadyToPayResultPtr response) {
    is_ready_to_pay_ = std::move(response);
  }

  void OnInvokePaymentAppResponse(mojom::InvokePaymentAppResultPtr response) {
    invoke_app_ = std::move(response);
  }

  void OnAbortPaymentAppResponse(bool response) { abort_app_ = response; }

  ArcPaymentAppBridgeTestSupport support_;
  mojom::IsPaymentImplementedResultPtr is_implemented_;
  mojom::IsReadyToPayResultPtr is_ready_to_pay_;
  mojom::InvokePaymentAppResultPtr invoke_app_;
  absl::optional<bool> abort_app_;
};

TEST_F(ArcPaymentAppBridgeTest, UnableToConnectInIsImplemented) {
  // Intentionally do not set an instance.

  EXPECT_CALL(*support_.instance(),
              IsPaymentImplemented(testing::_, testing::_))
      .Times(0);

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->IsPaymentImplemented(
          "com.example.app",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnPaymentImplementedResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_implemented_.is_null());
  EXPECT_FALSE(is_implemented_->is_valid());
  ASSERT_TRUE(is_implemented_->is_error());
  EXPECT_EQ("Unable to invoke Android apps.", is_implemented_->get_error());
}

TEST_F(ArcPaymentAppBridgeTest, IsImplemented) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(),
              IsPaymentImplemented(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const std::string& package_name,
             ArcPaymentAppBridge::IsPaymentImplementedCallback callback) {
            auto valid = mojom::IsPaymentImplementedValidResult::New();
            valid->activity_names.push_back("com.example.Activity");
            valid->service_names.push_back("com.example.Service");
            std::move(callback).Run(
                mojom::IsPaymentImplementedResult::NewValid(std::move(valid)));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->IsPaymentImplemented(
          "com.example.app",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnPaymentImplementedResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_implemented_.is_null());
  EXPECT_FALSE(is_implemented_->is_error());
  ASSERT_TRUE(is_implemented_->is_valid());
  ASSERT_FALSE(is_implemented_->get_valid().is_null());
  EXPECT_EQ(std::vector<std::string>{"com.example.Activity"},
            is_implemented_->get_valid()->activity_names);
  EXPECT_EQ(std::vector<std::string>{"com.example.Service"},
            is_implemented_->get_valid()->service_names);
}

TEST_F(ArcPaymentAppBridgeTest, IsNotImplemented) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(),
              IsPaymentImplemented(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const std::string& package_name,
             ArcPaymentAppBridge::IsPaymentImplementedCallback callback) {
            std::move(callback).Run(mojom::IsPaymentImplementedResult::NewValid(
                mojom::IsPaymentImplementedValidResult::New()));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->IsPaymentImplemented(
          "com.example.app",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnPaymentImplementedResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_implemented_.is_null());
  EXPECT_FALSE(is_implemented_->is_error());
  ASSERT_TRUE(is_implemented_->is_valid());
  ASSERT_FALSE(is_implemented_->get_valid().is_null());
  EXPECT_TRUE(is_implemented_->get_valid()->activity_names.empty());
  EXPECT_TRUE(is_implemented_->get_valid()->service_names.empty());
}

TEST_F(ArcPaymentAppBridgeTest, ImplementationCheckError) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(),
              IsPaymentImplemented(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const std::string& package_name,
             ArcPaymentAppBridge::IsPaymentImplementedCallback callback) {
            std::move(callback).Run(
                mojom::IsPaymentImplementedResult::NewError("Error message."));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->IsPaymentImplemented(
          "com.example.app",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnPaymentImplementedResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_implemented_.is_null());
  EXPECT_FALSE(is_implemented_->is_valid());
  ASSERT_TRUE(is_implemented_->is_error());
  EXPECT_EQ("Error message.", is_implemented_->get_error());
}

TEST_F(ArcPaymentAppBridgeTest, UnableToConnectInIsReadyToPay) {
  // Intentionally do not set an instance.

  EXPECT_CALL(*support_.instance(), IsReadyToPay(testing::_, testing::_))
      .Times(0);

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->IsReadyToPay(
          mojom::PaymentParameters::New(),
          base::BindOnce(&ArcPaymentAppBridgeTest::OnIsReadyToPayResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_ready_to_pay_.is_null());
  EXPECT_FALSE(is_ready_to_pay_->is_response());
  ASSERT_TRUE(is_ready_to_pay_->is_error());
  EXPECT_EQ("Unable to invoke Android apps.", is_ready_to_pay_->get_error());
}

TEST_F(ArcPaymentAppBridgeTest, IsReadyToPay) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(), IsReadyToPay(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](mojom::PaymentParametersPtr parameters,
             ArcPaymentAppBridge::IsReadyToPayCallback callback) {
            std::move(callback).Run(
                mojom::IsReadyToPayResult::NewResponse(true));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->IsReadyToPay(
          mojom::PaymentParameters::New(),
          base::BindOnce(&ArcPaymentAppBridgeTest::OnIsReadyToPayResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_ready_to_pay_.is_null());
  EXPECT_FALSE(is_ready_to_pay_->is_error());
  ASSERT_TRUE(is_ready_to_pay_->is_response());
  EXPECT_TRUE(is_ready_to_pay_->get_response());
}

TEST_F(ArcPaymentAppBridgeTest, IsNotReadyToPay) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(), IsReadyToPay(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](mojom::PaymentParametersPtr parameters,
             ArcPaymentAppBridge::IsReadyToPayCallback callback) {
            std::move(callback).Run(
                mojom::IsReadyToPayResult::NewResponse(false));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->IsReadyToPay(
          mojom::PaymentParameters::New(),
          base::BindOnce(&ArcPaymentAppBridgeTest::OnIsReadyToPayResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(is_ready_to_pay_.is_null());
  EXPECT_FALSE(is_ready_to_pay_->is_error());
  ASSERT_TRUE(is_ready_to_pay_->is_response());
  EXPECT_FALSE(is_ready_to_pay_->get_response());
}

TEST_F(ArcPaymentAppBridgeTest, UnableToConnectInInvokePaymentApp) {
  // Intentionally do not set an instance.

  EXPECT_CALL(*support_.instance(), InvokePaymentApp(testing::_, testing::_))
      .Times(0);

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->InvokePaymentApp(
          mojom::PaymentParameters::New(),
          base::BindOnce(&ArcPaymentAppBridgeTest::OnInvokePaymentAppResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(invoke_app_.is_null());
  EXPECT_FALSE(invoke_app_->is_valid());
  ASSERT_TRUE(invoke_app_->is_error());
  EXPECT_EQ("Unable to invoke Android apps.", invoke_app_->get_error());
}

TEST_F(ArcPaymentAppBridgeTest, InvokePaymentAppResultOK) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(), InvokePaymentApp(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](mojom::PaymentParametersPtr parameters,
             ArcPaymentAppBridge::InvokePaymentAppCallback callback) {
            auto valid = mojom::InvokePaymentAppValidResult::New();
            valid->is_activity_result_ok = true;
            valid->stringified_details = "{}";
            std::move(callback).Run(
                mojom::InvokePaymentAppResult::NewValid(std::move(valid)));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->InvokePaymentApp(
          mojom::PaymentParameters::New(),
          base::BindOnce(&ArcPaymentAppBridgeTest::OnInvokePaymentAppResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(invoke_app_.is_null());
  EXPECT_FALSE(invoke_app_->is_error());
  ASSERT_TRUE(invoke_app_->is_valid());
  ASSERT_FALSE(invoke_app_->get_valid().is_null());
  EXPECT_TRUE(invoke_app_->get_valid()->is_activity_result_ok);
  EXPECT_EQ("{}", invoke_app_->get_valid()->stringified_details);
}

TEST_F(ArcPaymentAppBridgeTest, InvokePaymentAppResultCancelled) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(), InvokePaymentApp(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](mojom::PaymentParametersPtr parameters,
             ArcPaymentAppBridge::InvokePaymentAppCallback callback) {
            auto valid = mojom::InvokePaymentAppValidResult::New();
            // User cancelled payment.
            valid->is_activity_result_ok = false;
            std::move(callback).Run(
                mojom::InvokePaymentAppResult::NewValid(std::move(valid)));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->InvokePaymentApp(
          mojom::PaymentParameters::New(),
          base::BindOnce(&ArcPaymentAppBridgeTest::OnInvokePaymentAppResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(invoke_app_.is_null());
  EXPECT_FALSE(invoke_app_->is_error());
  ASSERT_TRUE(invoke_app_->is_valid());
  ASSERT_FALSE(invoke_app_->get_valid().is_null());
  EXPECT_FALSE(invoke_app_->get_valid()->is_activity_result_ok);
}

TEST_F(ArcPaymentAppBridgeTest, InvokePaymentAppError) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(), InvokePaymentApp(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](mojom::PaymentParametersPtr parameters,
             ArcPaymentAppBridge::InvokePaymentAppCallback callback) {
            std::move(callback).Run(
                mojom::InvokePaymentAppResult::NewError("Error message."));
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->InvokePaymentApp(
          mojom::PaymentParameters::New(),
          base::BindOnce(&ArcPaymentAppBridgeTest::OnInvokePaymentAppResponse,
                         base::Unretained(this)));

  ASSERT_FALSE(invoke_app_.is_null());
  EXPECT_FALSE(invoke_app_->is_valid());
  ASSERT_TRUE(invoke_app_->is_error());
  EXPECT_EQ("Error message.", invoke_app_->get_error());
}

TEST_F(ArcPaymentAppBridgeTest, UnableToConnectAbortPaymentApp) {
  // Intentionally do not set an instance.

  EXPECT_CALL(*support_.instance(), AbortPaymentApp(testing::_, testing::_))
      .Times(0);

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->AbortPaymentApp(
          "some token",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnAbortPaymentAppResponse,
                         base::Unretained(this)));

  ASSERT_TRUE(abort_app_.has_value());
  ASSERT_FALSE(abort_app_.value());
}

TEST_F(ArcPaymentAppBridgeTest, AbortPaymentAppOK) {
  auto scoped_set_instance = support_.CreateScopedSetInstance();

  EXPECT_CALL(*support_.instance(), AbortPaymentApp(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [](const std::string& request_token,
             ArcPaymentAppBridge::AbortPaymentAppCallback callback) {
            std::move(callback).Run(true);
          }));

  ArcPaymentAppBridge::GetForBrowserContextForTesting(support_.context())
      ->AbortPaymentApp(
          "some token",
          base::BindOnce(&ArcPaymentAppBridgeTest::OnAbortPaymentAppResponse,
                         base::Unretained(this)));

  ASSERT_TRUE(abort_app_.has_value());
  ASSERT_TRUE(abort_app_.value());
}

}  // namespace
}  // namespace arc
