// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/payments/webapps/twa_package_helper.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace payments {

class PaymentRequestDialog;
class PaymentUIObserver;
class SecurePaymentConfirmationController;

class ChromePaymentRequestDelegate : public ContentPaymentRequestDelegate {
 public:
  explicit ChromePaymentRequestDelegate(
      content::RenderFrameHost* render_frame_host);

  ChromePaymentRequestDelegate(const ChromePaymentRequestDelegate&) = delete;
  ChromePaymentRequestDelegate& operator=(const ChromePaymentRequestDelegate&) =
      delete;

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
  autofill::AddressNormalizer* GetAddressNormalizer() override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  PrefService* GetPrefService() override;
  bool IsBrowserWindowActive() const override;

  // ContentPaymentRequestDelegate:
  content::RenderFrameHost* GetRenderFrameHost() const override;
  std::unique_ptr<webauthn::InternalAuthenticator> CreateInternalAuthenticator()
      const override;
  scoped_refptr<WebPaymentsWebDataService> GetWebPaymentsWebDataService()
      const override;
  PaymentRequestDisplayManager* GetDisplayManager() override;
  void EmbedPaymentHandlerWindow(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) override;
  bool IsInteractive() const override;
  std::string GetInvalidSslCertificateErrorMessage() override;
  void GetTwaPackageName(GetTwaPackageNameCallback callback) const override;
  PaymentRequestDialog* GetDialogForTesting() override;

  const base::WeakPtr<PaymentUIObserver> GetPaymentUIObserver() const override;
  std::string GetSecurePaymentConfirmationKeychainAccessGroup() const override;

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

  const content::GlobalRenderFrameHostId frame_routing_id_;

  TwaPackageHelper twa_package_helper_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_
