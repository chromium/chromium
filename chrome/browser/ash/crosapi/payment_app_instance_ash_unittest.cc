// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/crosapi/payment_app_instance_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/test/arc_payment_app_bridge_test_support.h"
#include "ash/public/cpp/external_arc/overlay/test/test_arc_overlay_manager.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/payments/mojom/payment_app.mojom.h"
#include "chromeos/components/payments/mojom/payment_app_types.mojom-forward.h"
#include "components/payments/core/android_app_description.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

class PaymentAppInstanceAshTest : public testing::Test {
 public:
  void SetUp() override {
    payment_app_instance_ash_ =
        std::make_unique<crosapi::PaymentAppInstanceAsh>();
    payment_app_instance_ash_->BindReceiver(
        payment_app_instance_remote_.BindNewPipeAndPassReceiver());
    scoped_set_instance_ = support_.CreateScopedSetInstance();
    payment_app_instance_ash_->SetPaymentAppServiceForTesting(
        arc::ArcPaymentAppBridge::GetForBrowserContextForTesting(
            support_.context()));
    payment_app_instance_ash_->SetInstanceRegistryForTesting(
        &instance_registry_);
  }
  base::UnguessableToken CreateTWAInstance() {
    aura::Window window(nullptr);
    window.Init(ui::LAYER_NOT_DRAWN);
    instance_registry_.CreateOrUpdateInstance(
        apps::InstanceParams("twa_app_id", &window));
    base::UnguessableToken twa_instance_id;
    instance_registry_.ForInstancesWithWindow(
        &window, [&twa_instance_id](const apps::InstanceUpdate& update) {
          twa_instance_id = update.InstanceId();
        });
    return twa_instance_id;
  }

 protected:
  arc::ArcPaymentAppBridgeTestSupport support_;
  mojo::Remote<chromeos::payments::mojom::PaymentAppInstance>
      payment_app_instance_remote_;
  std::unique_ptr<crosapi::PaymentAppInstanceAsh> payment_app_instance_ash_;
  std::unique_ptr<arc::ArcPaymentAppBridgeTestSupport::ScopedSetInstance>
      scoped_set_instance_;
  ash::TestArcOverlayManager overlay_manager_;
  apps::InstanceRegistry instance_registry_;
};

TEST_F(PaymentAppInstanceAshTest, IsPaymentImplemented) {
  EXPECT_CALL(*support_.instance(),
              IsPaymentImplemented(testing::_, testing::_))
      .WillOnce([](const std::string& package_name,
                   arc::ArcPaymentAppBridge::IsPaymentImplementedCallback
                       callback) {
        auto valid =
            chromeos::payments::mojom::IsPaymentImplementedValidResult::New();
        valid->activity_names.push_back("com.example.Activity");
        valid->service_names.push_back("com.example.Service");
        std::move(callback).Run(
            chromeos::payments::mojom::IsPaymentImplementedResult::NewValid(
                std::move(valid)));
      });

  base::test::TestFuture<
      chromeos::payments::mojom::IsPaymentImplementedResultPtr>
      future;
  payment_app_instance_ash_->IsPaymentImplemented("test_package_name",
                                                  future.GetCallback());

  auto is_payment_implemented_result = future.Take();
  ASSERT_TRUE(is_payment_implemented_result);
  ASSERT_FALSE(is_payment_implemented_result.is_null());
  EXPECT_FALSE(is_payment_implemented_result->is_error());
  ASSERT_TRUE(is_payment_implemented_result->is_valid());
  ASSERT_FALSE(is_payment_implemented_result->get_valid().is_null());
  EXPECT_EQ(std::vector<std::string>{"com.example.Activity"},
            is_payment_implemented_result->get_valid()->activity_names);
  EXPECT_EQ(std::vector<std::string>{"com.example.Service"},
            is_payment_implemented_result->get_valid()->service_names);
}

TEST_F(PaymentAppInstanceAshTest, IsPaymentImplementedError) {
  EXPECT_CALL(*support_.instance(),
              IsPaymentImplemented(testing::_, testing::_))
      .WillOnce([&](const std::string& package_name,
                    crosapi::PaymentAppInstanceAsh::IsPaymentImplementedCallback
                        callback) {
        std::move(callback).Run(
            chromeos::payments::mojom::IsPaymentImplementedResult::NewError(
                "Error message."));
      });

  base::test::TestFuture<
      chromeos::payments::mojom::IsPaymentImplementedResultPtr>
      future;
  payment_app_instance_ash_->IsPaymentImplemented("test_package_name",
                                                  future.GetCallback());

  auto is_payment_implemented_result = future.Take();
  ASSERT_TRUE(is_payment_implemented_result);
  ASSERT_FALSE(is_payment_implemented_result.is_null());
  EXPECT_FALSE(is_payment_implemented_result->is_valid());
  ASSERT_TRUE(is_payment_implemented_result->is_error());
  EXPECT_EQ("Error message.", is_payment_implemented_result->get_error());
}

TEST_F(PaymentAppInstanceAshTest, IsReadyToPay) {
  EXPECT_CALL(*support_.instance(), IsReadyToPay(testing::_, testing::_))
      .WillOnce(
          [](chromeos::payments::mojom::PaymentParametersPtr parameters,
             crosapi::PaymentAppInstanceAsh::IsReadyToPayCallback callback) {
            std::move(callback).Run(
                chromeos::payments::mojom::IsReadyToPayResult::NewResponse(
                    true));
          });

  base::test::TestFuture<chromeos::payments::mojom::IsReadyToPayResultPtr>
      future;
  payment_app_instance_ash_->IsReadyToPay(
      chromeos::payments::mojom::PaymentParameters::New(),
      future.GetCallback());

  auto is_ready_to_pay_result = future.Take();
  ASSERT_TRUE(is_ready_to_pay_result);
  ASSERT_FALSE(is_ready_to_pay_result.is_null());
  EXPECT_FALSE(is_ready_to_pay_result->is_error());
  ASSERT_TRUE(is_ready_to_pay_result->is_response());
  EXPECT_TRUE(is_ready_to_pay_result->get_response());
}

TEST_F(PaymentAppInstanceAshTest, IsNotReadyToPay) {
  EXPECT_CALL(*support_.instance(), IsReadyToPay(testing::_, testing::_))
      .WillOnce(
          [](chromeos::payments::mojom::PaymentParametersPtr parameters,
             crosapi::PaymentAppInstanceAsh::IsReadyToPayCallback callback) {
            std::move(callback).Run(
                chromeos::payments::mojom::IsReadyToPayResult::NewResponse(
                    false));
          });

  base::test::TestFuture<chromeos::payments::mojom::IsReadyToPayResultPtr>
      future;
  payment_app_instance_ash_->IsReadyToPay(
      chromeos::payments::mojom::PaymentParameters::New(),
      future.GetCallback());

  auto is_ready_to_pay_result = future.Take();
  ASSERT_TRUE(is_ready_to_pay_result);
  ASSERT_FALSE(is_ready_to_pay_result.is_null());
  EXPECT_FALSE(is_ready_to_pay_result->is_error());
  ASSERT_TRUE(is_ready_to_pay_result->is_response());
  EXPECT_FALSE(is_ready_to_pay_result->get_response());
}

TEST_F(PaymentAppInstanceAshTest, InvokePaymentAppNoToken) {
  base::test::TestFuture<chromeos::payments::mojom::InvokePaymentAppResultPtr>
      future;
  payment_app_instance_ash_->InvokePaymentApp(
      chromeos::payments::mojom::PaymentParameters::New(),
      future.GetCallback());

  auto invoke_payment_app_result = future.Take();
  ASSERT_TRUE(invoke_payment_app_result);
  ASSERT_FALSE(invoke_payment_app_result.is_null());
  EXPECT_FALSE(invoke_payment_app_result->is_valid());
  ASSERT_TRUE(invoke_payment_app_result->is_error());
  EXPECT_EQ("Unable to invoke Android apps.",
            invoke_payment_app_result->get_error());
}

TEST_F(PaymentAppInstanceAshTest, InvokePaymentAppResultOK) {
  EXPECT_CALL(*support_.instance(), InvokePaymentApp(testing::_, testing::_))
      .WillOnce([](chromeos::payments::mojom::PaymentParametersPtr parameters,
                   crosapi::PaymentAppInstanceAsh::InvokePaymentAppCallback
                       callback) {
        auto valid =
            chromeos::payments::mojom::InvokePaymentAppValidResult::New();
        valid->is_activity_result_ok = true;
        valid->stringified_details = "{}";
        std::move(callback).Run(
            chromeos::payments::mojom::InvokePaymentAppResult::NewValid(
                std::move(valid)));
      });

  auto params = chromeos::payments::mojom::PaymentParameters::New();
  params->request_token = "Abc";
  params->twa_instance_identifier = CreateTWAInstance();

  base::test::TestFuture<chromeos::payments::mojom::InvokePaymentAppResultPtr>
      future;
  payment_app_instance_ash_->InvokePaymentApp(std::move(params),
                                              future.GetCallback());

  auto invoke_payment_app_result = future.Take();
  ASSERT_TRUE(invoke_payment_app_result);
  ASSERT_FALSE(invoke_payment_app_result.is_null());
  EXPECT_FALSE(invoke_payment_app_result->is_error());
  ASSERT_TRUE(invoke_payment_app_result->is_valid());
  ASSERT_FALSE(invoke_payment_app_result->get_valid().is_null());
  EXPECT_TRUE(invoke_payment_app_result->get_valid()->is_activity_result_ok);
  EXPECT_EQ("{}", invoke_payment_app_result->get_valid()->stringified_details);
}

TEST_F(PaymentAppInstanceAshTest, InvokePaymentAppError) {
  EXPECT_CALL(*support_.instance(), InvokePaymentApp(testing::_, testing::_))
      .WillOnce([](chromeos::payments::mojom::PaymentParametersPtr parameters,
                   crosapi::PaymentAppInstanceAsh::InvokePaymentAppCallback
                       callback) {
        std::move(callback).Run(
            chromeos::payments::mojom::InvokePaymentAppResult::NewError(
                "Error message."));
      });

  auto params = chromeos::payments::mojom::PaymentParameters::New();
  params->request_token = "Abc";
  params->twa_instance_identifier = CreateTWAInstance();

  base::test::TestFuture<chromeos::payments::mojom::InvokePaymentAppResultPtr>
      future;
  payment_app_instance_ash_->InvokePaymentApp(std::move(params),
                                              future.GetCallback());

  auto invoke_payment_app_result = future.Take();
  ASSERT_TRUE(invoke_payment_app_result);
  ASSERT_FALSE(invoke_payment_app_result.is_null());
  EXPECT_FALSE(invoke_payment_app_result->is_valid());
  ASSERT_TRUE(invoke_payment_app_result->is_error());
  EXPECT_EQ("Error message.", invoke_payment_app_result->get_error());
}

TEST_F(PaymentAppInstanceAshTest, AbortPaymentAppOK) {
  EXPECT_CALL(*support_.instance(), AbortPaymentApp(testing::_, testing::_))
      .WillOnce(
          [](const std::string& request_token,
             crosapi::PaymentAppInstanceAsh::AbortPaymentAppCallback callback) {
            std::move(callback).Run(true);
          });

  base::test::TestFuture<bool> future;
  payment_app_instance_ash_->AbortPaymentApp("some token",
                                             future.GetCallback());

  auto abort_app_result = future.Take();
  ASSERT_TRUE(abort_app_result);
}

}  // namespace
}  // namespace payments
