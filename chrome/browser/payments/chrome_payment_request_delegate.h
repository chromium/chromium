// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/secure_payment_confirmation_controller.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {

class PaymentRequestDialog;
class PaymentUIObserver;

class ChromePaymentRequestDelegate : public ContentPaymentRequestDelegate {
 public:
  explicit ChromePaymentRequestDelegate(
      content::RenderFrameHost* render_frame_host);
  ~ChromePaymentRequestDelegate() override;

  // PaymentRequestDelegate:
  void ShowDialog(base::WeakPtr<PaymentRequest> request) override;
  void RetryDialog() override;
  void CloseDialog() override;
  void ShowErrorMessage() override;
  void ShowProcessingSpinner() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  const std::string& GetApplicationLocale() const override;
  bool IsOffTheRecord() const override;
  const GURL& GetLastCommittedURL() const override;
  void DoFullCardRequest(
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate) override;
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  std::string GetAuthenticatedEmail() const override;
  PrefService* GetPrefService() override;
  bool IsBrowserWindowActive() const override;

  // ContentPaymentRequestDelegate:
  std::unique_ptr<autofill::InternalAuthenticator> CreateInternalAuthenticator()
      const override;
  scoped_refptr<PaymentManifestWebDataService>
  GetPaymentManifestWebDataService() const override;
  PaymentRequestDisplayManager* GetDisplayManager() override;
  void EmbedPaymentHandlerWindow(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) override;
  bool IsInteractive() const override;
  std::string GetInvalidSslCertificateErrorMessage() override;
  bool SkipUiForBasicCard() const override;
  std::string GetTwaPackageName() const override;
  PaymentRequestDialog* GetDialogForTesting() override;
  const PaymentUIObserver* GetPaymentUIObserver() const override;

 protected:
  // Reference to the dialog so that we can satisfy calls to CloseDialog(). This
  // reference is invalid once CloseDialog() has been called on it, because the
  // dialog will be destroyed. Some implementations are owned by the views::
  // dialog machinery. Protected for testing.
  base::WeakPtr<PaymentRequestDialog> shown_dialog_;

 private:
  // Returns the browser context of the `render_frame_host_` or null if not
  // available.
  content::BrowserContext* GetBrowserContextOrNull() const;

  std::unique_ptr<SecurePaymentConfirmationController> spc_dialog_;

  content::GlobalFrameRoutingId frame_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromePaymentRequestDelegate);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_
