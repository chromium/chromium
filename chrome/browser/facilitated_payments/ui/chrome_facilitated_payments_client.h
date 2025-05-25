// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"
#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/multiple_request_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class BankAccount;
class Ewallet;
class StrikeDatabase;
}  // namespace autofill

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

// Chrome implementation of `FacilitatedPaymentsClient`. `WebContents` owns 1
// instance of this class. Creates and owns
// `ContentFacilitatedPaymentsDriverFactory`.
class ChromeFacilitatedPaymentsClient
    : public payments::facilitated::FacilitatedPaymentsClient,
      public content::WebContentsUserData<ChromeFacilitatedPaymentsClient> {
 public:
  ChromeFacilitatedPaymentsClient(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  ChromeFacilitatedPaymentsClient(const ChromeFacilitatedPaymentsClient&) =
      delete;
  ChromeFacilitatedPaymentsClient& operator=(
      const ChromeFacilitatedPaymentsClient&) = delete;
  ~ChromeFacilitatedPaymentsClient() override;

  // RiskDataLoader:
  void LoadRiskData(base::OnceCallback<void(const std::string&)>
                        on_risk_data_loaded_callback) final;

  payments::facilitated::ContentFacilitatedPaymentsDriver*
  GetFacilitatedPaymentsDriverForFrame(
      content::RenderFrameHost* render_frame_host);

  virtual void SetFacilitatedPaymentsControllerForTesting(
      std::unique_ptr<FacilitatedPaymentsController> controller);

 private:
  friend class content::WebContentsUserData<ChromeFacilitatedPaymentsClient>;

  // FacilitatedPaymentsClient:
  // This returns nullptr if the `Profile` associated is null.
  autofill::PaymentsDataManager* GetPaymentsDataManager() final;
  // This returns nullptr if the `Profile` associated is null.
  payments::facilitated::FacilitatedPaymentsNetworkInterface*
  GetFacilitatedPaymentsNetworkInterface() final;
  payments::facilitated::MultipleRequestFacilitatedPaymentsNetworkInterface*
  GetMultipleRequestFacilitatedPaymentsNetworkInterface() final;
  // This returns std::nullopt if the `Profile` associated is null.
  std::optional<CoreAccountInfo> GetCoreAccountInfo() final;
  bool IsInLandscapeMode() final;
  bool IsFoldable() final;
  optimization_guide::OptimizationGuideDecider* GetOptimizationGuideDecider()
      final;
  void ShowPixPaymentPrompt(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(int64_t)> on_payment_account_selected) final;
  void ShowEwalletPaymentPrompt(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      base::OnceCallback<void(int64_t)> on_payment_account_selected) final;
  void ShowProgressScreen() final;
  void ShowErrorScreen() final;
  void DismissPrompt() final;
  void SetUiEventListener(
      base::RepeatingCallback<void(payments::facilitated::UiEvent)>
          ui_event_listener) final;
  autofill::StrikeDatabase* GetStrikeDatabase() final;
  bool IsPixAccountLinkingSupported() const final;
  void ShowPixAccountLinkingPrompt() final;

  // Register any allowlists with the OptimizationGuide framework, so that
  // individual features can later request to check whether the current main
  // frame URL is eligible for that feature.
  void RegisterAllowlists();

  payments::facilitated::ContentFacilitatedPaymentsDriverFactory
      driver_factory_;

  std::unique_ptr<payments::facilitated::FacilitatedPaymentsNetworkInterface>
      facilitated_payments_network_interface_;
  std::unique_ptr<
      payments::facilitated::MultipleRequestFacilitatedPaymentsNetworkInterface>
      multiple_request_facilitated_payments_network_interface_;

  std::unique_ptr<FacilitatedPaymentsController>
      facilitated_payments_controller_;

  // The optimization guide decider to help determine whether the current main
  // frame URL is eligible for facilitated payments.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_
