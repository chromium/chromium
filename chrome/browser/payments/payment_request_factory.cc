// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_request_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/core/payment_request_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace payments {

namespace {

using PaymentRequestFactoryCallback = base::RepeatingCallback<void(
    mojo::PendingReceiver<mojom::PaymentRequest> receiver,
    content::RenderFrameHost* render_frame_host)>;

PaymentRequestFactoryCallback& GetTestingFactoryCallback() {
  static base::NoDestructor<PaymentRequestFactoryCallback> callback;
  return *callback;
}

// Measures whether users have the "Allow sites to check if you have payment
// methods saved" toggle enabled or disabled.
//
// This is recorded only once per BrowserContext, when the first PaymentRequest
// object is created in that browsing session. The goal is to sub-select the
// metric to users who are in a payments context, as opposed to the general
// population that is measured by the
// PaymentRequest.IsCanMakePaymentAllowedByPref.Startup histogram.
void RecordCanMakePaymentAllowedHistogram(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || !profile->GetPrefs()) {
    return;
  }

  // The Profile pointers in this set are only used to avoid duplicate-counting,
  // and may no longer be live - they should NEVER be dereferenced!
  static base::NoDestructor<base::flat_set<Profile*>> recorded_profiles;
  if (recorded_profiles->contains(profile)) {
    return;
  }
  recorded_profiles->insert(profile);

  RecordCanMakePaymentPrefMetrics(*profile->GetPrefs(),
                                  "PaymentRequestConstruction.Once");
}

}  // namespace

void CreatePaymentRequest(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::PaymentRequest> receiver) {
  if (!render_frame_host->IsActive()) {
    // This happens when the page has navigated away, which would cause the
    // blink PaymentRequest to be released shortly, or when the iframe is being
    // removed from the page, which is not a use case that we support.
    // Abandoning the `receiver` will close the mojo connection, so blink
    // PaymentRequest will receive a connection error and will clean up itself.
    return;
  }

  if (!render_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kPayment)) {
    mojo::ReportBadMessage("Permissions policy blocks Payment");
    return;
  }

  RecordCanMakePaymentAllowedHistogram(render_frame_host->GetBrowserContext());

  if (GetTestingFactoryCallback()) {
    return GetTestingFactoryCallback().Run(std::move(receiver),
                                           render_frame_host);
  }

  // PaymentRequest is a DocumentService, whose lifetime is managed by the
  // RenderFrameHost passed in here.
  auto delegate =
      std::make_unique<ChromePaymentRequestDelegate>(render_frame_host);
  new PaymentRequest(std::move(delegate), std::move(receiver));
}

void SetPaymentRequestFactoryForTesting(
    PaymentRequestFactoryCallback factory_callback) {
  GetTestingFactoryCallback() = std::move(factory_callback);
}

}  // namespace payments
