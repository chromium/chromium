// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_controller.h"
#include "components/facilitated_payments/android/device_delegate_android.h"
#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "content/public/browser/web_contents_user_data.h"

namespace url {
class Origin;
}  // namespace url

namespace autofill {
class BankAccount;
class Ewallet;
}  // namespace autofill

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace strike_database {
class StrikeDatabase;
}  // namespace strike_database

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
  const url::Origin& GetLastCommittedOrigin() const final;
  // This returns nullptr if the `Profile` associated is null.
  autofill::PaymentsDataManager* GetPaymentsDataManager() final;
  // This returns nullptr if the `Profile` associated is null.
  payments::facilitated::FacilitatedPaymentsNetworkInterface*
  GetFacilitatedPaymentsNetworkInterface() final;
  // This returns std::nullopt if the `Profile` associated is null.
  std::optional<CoreAccountInfo> GetCoreAccountInfo() final;
  bool IsInLandscapeMode() final;
  bool IsFoldable() final;
  bool IsInChromeCustomTabMode() final;
  optimization_guide::OptimizationGuideDecider* GetOptimizationGuideDecider()
      final;
  payments::facilitated::DeviceDelegate* GetDeviceDelegate() final;
  bool IsWebContentsVisibleOrOccluded() final;
  void ShowPixPaymentPrompt(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(int64_t)> on_payment_account_selected) final;
  void ShowPaymentLinkPrompt(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      std::unique_ptr<payments::facilitated::FacilitatedPaymentsAppInfoList>
          app_suggestions,
      base::OnceCallback<void(payments::facilitated::SelectedFopData)>
          on_fop_selected) final;
  void ShowProgressScreen() final;
  void ShowErrorScreen() final;
  void DismissPrompt() final;
  void SetUiEventListener(
      base::RepeatingCallback<void(payments::facilitated::UiEvent)>
          ui_event_listener) final;
  strike_database::StrikeDatabase* GetStrikeDatabase() final;
  void InitPixAccountLinkingFlow(
      const url::Origin& pix_payment_page_origin) final;
  void ShowPixAccountLinkingPrompt(
      base::OnceCallback<void()> on_accepted,
      base::OnceCallback<void()> on_declined) final;
  bool HasScreenlockOrBiometricSetup() final;

  // Register any allowlists with the OptimizationGuide framework, so that
  // individual features can later request to check whether the current main
  // frame URL is eligible for that feature.
  void RegisterAllowlists();

  payments::facilitated::ContentFacilitatedPaymentsDriverFactory
      driver_factory_;

  std::unique_ptr<payments::facilitated::FacilitatedPaymentsNetworkInterface>
      facilitated_payments_network_interface_;

  std::unique_ptr<FacilitatedPaymentsController>
      facilitated_payments_controller_;

  // The optimization guide decider to help determine whether the current main
  // frame URL is eligible for facilitated payments.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  payments::facilitated::DeviceDelegateAndroid device_delegate_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_
