// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "components/payments/content/android/payment_app_service_bridge.h"
#include "components/payments/content/payment_app_service.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/const_csp_checker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace payments {

class MockCallback {
 public:
  MockCallback() = default;
  MOCK_METHOD1(NotifyPaymentAppCreated, void(std::unique_ptr<PaymentApp> app));
  MOCK_METHOD1(NotifyCanMakePaymentCalculated, void(bool can_make_payment));
  MOCK_METHOD2(NotifyPaymentAppCreationError,
               void(const std::string& error, AppCreationFailureReason reason));
  MOCK_METHOD0(NotifyDoneCreatingPaymentApps, void(void));
  MOCK_METHOD0(SetCanMakePaymentEvenWithoutApps, void(void));
  MOCK_METHOD0(SetOptOutOffered, void(void));
};

class MockApp : public PaymentApp {
 public:
  MockApp()
      : PaymentApp(/*icon_resource_id=*/0,
                   PaymentApp::Type::SERVICE_WORKER_APP) {}

  ~MockApp() override = default;

  // PaymentApp implementation:
  MOCK_METHOD1(InvokePaymentApp, void(base::WeakPtr<Delegate> delegate));
  MOCK_CONST_METHOD0(IsCompleteForPayment, bool());
  MOCK_CONST_METHOD0(CanPreselect, bool());
  MOCK_CONST_METHOD0(GetMissingInfoLabel, std::u16string());
  MOCK_CONST_METHOD0(HasEnrolledInstrument, bool());
  MOCK_METHOD0(RecordUse, void());
  MOCK_CONST_METHOD0(NeedsInstallation, bool());
  MOCK_CONST_METHOD0(GetId, std::string());
  MOCK_CONST_METHOD0(GetLabel, std::u16string());
  MOCK_CONST_METHOD0(GetSublabel, std::u16string());
  MOCK_CONST_METHOD1(IsValidForModifier, bool(const std::string& method));
  MOCK_METHOD0(AsWeakPtr, base::WeakPtr<PaymentApp>());
  MOCK_CONST_METHOD0(HandlesShippingAddress, bool());
  MOCK_CONST_METHOD0(HandlesPayerName, bool());
  MOCK_CONST_METHOD0(HandlesPayerEmail, bool());
  MOCK_CONST_METHOD0(HandlesPayerPhone, bool());
};

class PaymentAppServiceBridgeUnitTest
    : public ::testing::TestWithParam<std::string> {
 public:
  PaymentAppServiceBridgeUnitTest()
      : top_origin_("https://shop.example"),
        frame_origin_("https://merchant.example") {
    web_contents_ =
        test_web_contents_factory_.CreateWebContents(&browser_context_);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents_,
                                                               frame_origin_);
  }

  mojom::PaymentMethodDataPtr MakePaymentMethodData(
      const std::string& supported_method) {
    mojom::PaymentMethodDataPtr out = mojom::PaymentMethodData::New();
    out->supported_method = supported_method;
    return out;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile browser_context_;
  content::TestWebContentsFactory test_web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  GURL top_origin_;
  GURL frame_origin_;
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
};

TEST_P(PaymentAppServiceBridgeUnitTest, Smoke) {
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(MakePaymentMethodData("basic-card"));
  method_data.push_back(MakePaymentMethodData("https://ph.example"));
  PaymentRequestSpec spec(mojom::PaymentOptions::New(),
                          mojom::PaymentDetails::New(), std::move(method_data),
                          /*observer=*/nullptr, /*app_locale=*/"en-US");

  ConstCSPChecker const_csp_checker(/*allow=*/true);
  MockCallback mock_callback;
  base::WeakPtr<PaymentAppServiceBridge> bridge =
      PaymentAppServiceBridge::Create(
          std::make_unique<PaymentAppService>(
              web_contents_->GetBrowserContext()),
          web_contents_->GetPrimaryMainFrame(), top_origin_, spec.AsWeakPtr(),
          /*twa_package_name=*/GetParam(), web_data_service_,
          /*is_off_the_record=*/false, const_csp_checker.GetWeakPtr(),
          base::BindRepeating(&MockCallback::NotifyCanMakePaymentCalculated,
                              base::Unretained(&mock_callback)),
          base::BindRepeating(&MockCallback::NotifyPaymentAppCreated,
                              base::Unretained(&mock_callback)),
          base::BindRepeating(&MockCallback::NotifyPaymentAppCreationError,
                              base::Unretained(&mock_callback)),
          base::BindOnce(&MockCallback::NotifyDoneCreatingPaymentApps,
                         base::Unretained(&mock_callback)),
          base::BindRepeating(&MockCallback::SetCanMakePaymentEvenWithoutApps,
                              base::Unretained(&mock_callback)),
          base::BindRepeating(&MockCallback::SetOptOutOffered,
                              base::Unretained(&mock_callback)))
          ->GetWeakPtrForTest();

  EXPECT_EQ(web_contents_, bridge->GetWebContents());
  EXPECT_EQ(top_origin_, bridge->GetTopOrigin());
  EXPECT_EQ(frame_origin_, bridge->GetFrameOrigin());
  EXPECT_EQ("https://merchant.example",
            bridge->GetFrameSecurityOrigin().Serialize());
  EXPECT_EQ(web_contents_->GetPrimaryMainFrame(),
            bridge->GetInitiatorRenderFrameHost());
  EXPECT_EQ(2U, bridge->GetMethodData().size());
  EXPECT_EQ("basic-card", bridge->GetMethodData()[0]->supported_method);
  EXPECT_EQ("https://ph.example", bridge->GetMethodData()[1]->supported_method);

  auto app = std::make_unique<MockApp>();
  EXPECT_CALL(mock_callback, NotifyPaymentAppCreated(::testing::_));
  bridge->OnPaymentAppCreated(std::move(app));

  EXPECT_CALL(mock_callback, SetCanMakePaymentEvenWithoutApps());
  bridge->SetCanMakePaymentEvenWithoutApps();

  EXPECT_CALL(mock_callback, SetOptOutOffered());
  bridge->SetOptOutOffered();

  EXPECT_CALL(mock_callback,
              NotifyPaymentAppCreationError("some error",
                                            AppCreationFailureReason::UNKNOWN));
  bridge->OnPaymentAppCreationError("some error",
                                    AppCreationFailureReason::UNKNOWN);

  // NotifyDoneCreatingPaymentApps() is only called after
  // OnDoneCreatingPaymentApps() is called for each payment factories in
  // |bridge|.
  bridge->OnDoneCreatingPaymentApps();
  bridge->OnDoneCreatingPaymentApps();

  EXPECT_CALL(mock_callback, NotifyDoneCreatingPaymentApps());
  bridge->OnDoneCreatingPaymentApps();

  // |bridge| cleans itself up after NotifyDoneCreatingPaymentApps().
  CHECK_EQ(nullptr, bridge.get());
}

// An empty string indicates running outside of a TWA. A non-empty string is the
// package name of the TWA when running in a TWA mode.
INSTANTIATE_TEST_SUITE_P(WithAndWithoutTwaPackageName,
                         PaymentAppServiceBridgeUnitTest,
                         ::testing::Values("", "com.example.twa.app"));

}  // namespace payments
