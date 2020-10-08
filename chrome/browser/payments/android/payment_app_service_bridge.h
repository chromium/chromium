// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_ANDROID_PAYMENT_APP_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_PAYMENTS_ANDROID_PAYMENT_APP_SERVICE_BRIDGE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app_factory.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace payments {

// A bridge that holds parameters needed by PaymentAppService and redirects
// callbacks from PaymentAppFactory to callbacks set by the caller.
class PaymentAppServiceBridge : public PaymentAppFactory::Delegate {
 public:
  using CanMakePaymentCalculatedCallback = base::OnceCallback<void(bool)>;
  using PaymentAppCreatedCallback =
      base::RepeatingCallback<void(std::unique_ptr<PaymentApp>)>;
  using PaymentAppCreationErrorCallback =
      base::RepeatingCallback<void(const std::string&)>;

  // Creates a new PaymentAppServiceBridge. This object is self-deleting; its
  // memory is freed when OnDoneCreatingPaymentApps() is called
  // `number_of_factories` times. The `spec` parameter should not be null.
  static PaymentAppServiceBridge* Create(
      size_t number_of_factories,
      content::RenderFrameHost* render_frame_host,
      const GURL& top_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      const std::string& twa_package_name,
      scoped_refptr<PaymentManifestWebDataService> web_data_service,
      bool may_crawl_for_installable_payment_apps,
      CanMakePaymentCalculatedCallback can_make_payment_calculated_callback,
      PaymentAppCreatedCallback payment_app_created_callback,
      PaymentAppCreationErrorCallback payment_app_creation_error_callback,
      base::OnceClosure done_creating_payment_apps_callback);

  ~PaymentAppServiceBridge() override;

  // Not copiable or movable.
  PaymentAppServiceBridge(const PaymentAppServiceBridge&) = delete;
  PaymentAppServiceBridge& operator=(const PaymentAppServiceBridge&) = delete;

  base::WeakPtr<PaymentAppServiceBridge> GetWeakPtr();

  // PaymentAppFactory::Delegate
  content::WebContents* GetWebContents() override;
  const GURL& GetTopOrigin() override;
  const GURL& GetFrameOrigin() override;
  const url::Origin& GetFrameSecurityOrigin() override;
  content::RenderFrameHost* GetInitiatorRenderFrameHost() const override;
  const std::vector<mojom::PaymentMethodDataPtr>& GetMethodData()
      const override;
  std::unique_ptr<autofill::InternalAuthenticator> CreateInternalAuthenticator()
      const override;
  scoped_refptr<PaymentManifestWebDataService>
  GetPaymentManifestWebDataService() const override;
  bool MayCrawlForInstallablePaymentApps() override;
  bool IsOffTheRecord() const override;
  const std::vector<autofill::AutofillProfile*>& GetBillingProfiles() override;
  bool IsRequestedAutofillDataAvailable() override;
  ContentPaymentRequestDelegate* GetPaymentRequestDelegate() const override;
  void ShowProcessingSpinner() override;
  base::WeakPtr<PaymentRequestSpec> GetSpec() const override;
  std::string GetTwaPackageName() const override;
  void OnPaymentAppCreated(std::unique_ptr<PaymentApp> app) override;
  void OnPaymentAppCreationError(const std::string& error_message) override;
  bool SkipCreatingNativePaymentApps() const override;
  void OnDoneCreatingPaymentApps() override;
  void SetCanMakePaymentEvenWithoutApps() override;

 private:
  // Prevents direct instantiation. Callers should use Create() instead. The
  // `spec` parameter should not be null.
  PaymentAppServiceBridge(
      size_t number_of_factories,
      content::RenderFrameHost* render_frame_host,
      const GURL& top_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      const std::string& twa_package_name,
      scoped_refptr<PaymentManifestWebDataService> web_data_service,
      bool may_crawl_for_installable_payment_apps,
      CanMakePaymentCalculatedCallback can_make_payment_calculated_callback,
      PaymentAppCreatedCallback payment_app_created_callback,
      PaymentAppCreationErrorCallback payment_app_creation_error_callback,
      base::OnceClosure done_creating_payment_apps_callback);

  size_t number_of_pending_factories_;
  content::GlobalFrameRoutingId frame_routing_id_;
  const GURL top_origin_;
  const GURL frame_origin_;
  const url::Origin frame_security_origin_;
  base::WeakPtr<PaymentRequestSpec> spec_;
  const std::string twa_package_name_;
  scoped_refptr<PaymentManifestWebDataService>
      payment_manifest_web_data_service_;
  bool may_crawl_for_installable_payment_apps_;
  std::vector<autofill::AutofillProfile*> dummy_profiles_;

  CanMakePaymentCalculatedCallback can_make_payment_calculated_callback_;
  PaymentAppCreatedCallback payment_app_created_callback_;
  PaymentAppCreationErrorCallback payment_app_creation_error_callback_;
  base::OnceClosure done_creating_payment_apps_callback_;

  base::WeakPtrFactory<PaymentAppServiceBridge> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_ANDROID_PAYMENT_APP_SERVICE_BRIDGE_H_
