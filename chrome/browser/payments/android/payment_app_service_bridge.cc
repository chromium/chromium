// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/android/payment_app_service_bridge.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "chrome/browser/autofill/android/internal_authenticator_android.h"
#include "chrome/browser/payments/android/jni_headers/PaymentAppServiceBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/android/jni_payment_app.h"
#include "components/payments/content/android/payment_request_spec.h"
#include "components/payments/content/payment_app_service.h"
#include "components/payments/content/payment_app_service_factory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

namespace {
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::payments::mojom::PaymentMethodDataPtr;

void OnCanMakePaymentCalculated(const JavaRef<jobject>& jcallback,
                                bool can_make_payment) {
  Java_PaymentAppServiceCallback_onCanMakePaymentCalculated(
      AttachCurrentThread(), jcallback, can_make_payment);
}

void OnPaymentAppCreated(const JavaRef<jobject>& jcallback,
                         std::unique_ptr<payments::PaymentApp> payment_app) {
  JNIEnv* env = AttachCurrentThread();
  Java_PaymentAppServiceCallback_onPaymentAppCreated(
      env, jcallback,
      payments::JniPaymentApp::Create(env, std::move(payment_app)));
}

void OnPaymentAppCreationError(const JavaRef<jobject>& jcallback,
                               const std::string& error_message) {
  JNIEnv* env = AttachCurrentThread();
  Java_PaymentAppServiceCallback_onPaymentAppCreationError(
      env, jcallback, ConvertUTF8ToJavaString(env, error_message));
}

void OnDoneCreatingPaymentApps(const JavaRef<jobject>& jcallback) {
  JNIEnv* env = AttachCurrentThread();
  Java_PaymentAppServiceCallback_onDoneCreatingPaymentApps(env, jcallback);
}

}  // namespace

/* static */
void JNI_PaymentAppServiceBridge_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& jrender_frame_host,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jobject>& jpayment_request_spec,
    const JavaParamRef<jstring>& jtwa_package_name,
    jboolean jmay_crawl_for_installable_payment_apps,
    const JavaParamRef<jobject>& jcallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  if (!render_frame_host)  // The frame is being unloaded.
    return;

  std::string top_origin = ConvertJavaStringToUTF8(jtop_origin);

  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
          Profile::FromBrowserContext(render_frame_host->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS);

  payments::PaymentAppService* service =
      payments::PaymentAppServiceFactory::GetForContext(
          render_frame_host->GetBrowserContext());
  auto* bridge = payments::PaymentAppServiceBridge::Create(
      service->GetNumberOfFactories(), render_frame_host, GURL(top_origin),
      payments::android::PaymentRequestSpec::FromJavaPaymentRequestSpec(
          env, jpayment_request_spec),
      jtwa_package_name ? ConvertJavaStringToUTF8(env, jtwa_package_name) : "",
      web_data_service, jmay_crawl_for_installable_payment_apps,
      base::BindOnce(&OnCanMakePaymentCalculated,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindRepeating(&OnPaymentAppCreated,
                          ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindRepeating(&OnPaymentAppCreationError,
                          ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindOnce(&OnDoneCreatingPaymentApps,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));

  service->Create(bridge->GetWeakPtr());
}

namespace payments {
namespace {

// A singleton class to maintain  ownership of PaymentAppServiceBridge objects
// until Remove() is called.
class PaymentAppServiceBridgeStorage {
 public:
  static PaymentAppServiceBridgeStorage* GetInstance() {
    return base::Singleton<PaymentAppServiceBridgeStorage>::get();
  }

  PaymentAppServiceBridge* Add(std::unique_ptr<PaymentAppServiceBridge> owned) {
    DCHECK(owned);
    PaymentAppServiceBridge* key = owned.get();
    owner_[key] = std::move(owned);
    return key;
  }

  void Remove(PaymentAppServiceBridge* owned) {
    size_t number_of_deleted_objects = owner_.erase(owned);
    DCHECK_EQ(1U, number_of_deleted_objects);
  }

 private:
  friend struct base::DefaultSingletonTraits<PaymentAppServiceBridgeStorage>;
  PaymentAppServiceBridgeStorage() = default;
  ~PaymentAppServiceBridgeStorage() = default;

  std::map<PaymentAppServiceBridge*, std::unique_ptr<PaymentAppServiceBridge>>
      owner_;
};

}  // namespace

/* static */
PaymentAppServiceBridge* PaymentAppServiceBridge::Create(
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
    base::OnceClosure done_creating_payment_apps_callback) {
  DCHECK(render_frame_host);
  // Not using std::make_unique, because that requires a public constructor.
  std::unique_ptr<PaymentAppServiceBridge> bridge(new PaymentAppServiceBridge(
      number_of_factories, render_frame_host, top_origin, spec,
      twa_package_name, std::move(web_data_service),
      may_crawl_for_installable_payment_apps,
      std::move(can_make_payment_calculated_callback),
      std::move(payment_app_created_callback),
      std::move(payment_app_creation_error_callback),
      std::move(done_creating_payment_apps_callback)));
  return PaymentAppServiceBridgeStorage::GetInstance()->Add(std::move(bridge));
}

PaymentAppServiceBridge::PaymentAppServiceBridge(
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
    base::OnceClosure done_creating_payment_apps_callback)
    : number_of_pending_factories_(number_of_factories),
      frame_routing_id_(content::GlobalFrameRoutingId(
          render_frame_host->GetProcess()->GetID(),
          render_frame_host->GetRoutingID())),
      top_origin_(top_origin),
      frame_origin_(url_formatter::FormatUrlForSecurityDisplay(
          render_frame_host->GetLastCommittedURL())),
      frame_security_origin_(render_frame_host->GetLastCommittedOrigin()),
      spec_(spec),
      twa_package_name_(twa_package_name),
      payment_manifest_web_data_service_(web_data_service),
      may_crawl_for_installable_payment_apps_(
          may_crawl_for_installable_payment_apps),
      can_make_payment_calculated_callback_(
          std::move(can_make_payment_calculated_callback)),
      payment_app_created_callback_(std::move(payment_app_created_callback)),
      payment_app_creation_error_callback_(
          std::move(payment_app_creation_error_callback)),
      done_creating_payment_apps_callback_(
          std::move(done_creating_payment_apps_callback)) {}

PaymentAppServiceBridge::~PaymentAppServiceBridge() = default;

base::WeakPtr<PaymentAppServiceBridge> PaymentAppServiceBridge::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* PaymentAppServiceBridge::GetWebContents() {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh && rfh->IsCurrent()
             ? content::WebContents::FromRenderFrameHost(rfh)
             : nullptr;
}
const GURL& PaymentAppServiceBridge::GetTopOrigin() {
  return top_origin_;
}

const GURL& PaymentAppServiceBridge::GetFrameOrigin() {
  return frame_origin_;
}

const url::Origin& PaymentAppServiceBridge::GetFrameSecurityOrigin() {
  return frame_security_origin_;
}

content::RenderFrameHost* PaymentAppServiceBridge::GetInitiatorRenderFrameHost()
    const {
  return content::RenderFrameHost::FromID(frame_routing_id_);
}

const std::vector<PaymentMethodDataPtr>&
PaymentAppServiceBridge::GetMethodData() const {
  DCHECK(spec_);
  return spec_->method_data();
}

std::unique_ptr<autofill::InternalAuthenticator>
PaymentAppServiceBridge::CreateInternalAuthenticator() const {
  // This authenticator can be used in a cross-origin iframe only if the
  // top-level frame allowed it with Permissions Policy, e.g., with
  // allow="payment" iframe attribute. The secure payment confirmation dialog
  // displays the top-level origin in its UI before the user can click on the
  // [Verify] button to invoke this authenticator.
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh && rfh->IsCurrent()
             ? std::make_unique<InternalAuthenticatorAndroid>(
                   rfh->GetMainFrame())
             : nullptr;
}

scoped_refptr<PaymentManifestWebDataService>
PaymentAppServiceBridge::GetPaymentManifestWebDataService() const {
  return payment_manifest_web_data_service_;
}

bool PaymentAppServiceBridge::MayCrawlForInstallablePaymentApps() {
  return may_crawl_for_installable_payment_apps_;
}

bool PaymentAppServiceBridge::IsOffTheRecord() const {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh)
    return false;
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  return profile && profile->IsOffTheRecord();
}

const std::vector<autofill::AutofillProfile*>&
PaymentAppServiceBridge::GetBillingProfiles() {
  // PaymentAppService flow should have short-circuited before this point.
  NOTREACHED();
  return dummy_profiles_;
}

bool PaymentAppServiceBridge::IsRequestedAutofillDataAvailable() {
  // PaymentAppService flow should have short-circuited before this point.
  NOTREACHED();
  return false;
}

ContentPaymentRequestDelegate*
PaymentAppServiceBridge::GetPaymentRequestDelegate() const {
  // PaymentAppService flow should have short-circuited before this point.
  NOTREACHED();
  return nullptr;
}

void PaymentAppServiceBridge::ShowProcessingSpinner() {
  // Java UI determines when the show a spinner itself.
}

base::WeakPtr<PaymentRequestSpec> PaymentAppServiceBridge::GetSpec() const {
  return spec_;
}

std::string PaymentAppServiceBridge::GetTwaPackageName() const {
  return twa_package_name_;
}

void PaymentAppServiceBridge::OnPaymentAppCreated(
    std::unique_ptr<PaymentApp> app) {
  if (can_make_payment_calculated_callback_)
    std::move(can_make_payment_calculated_callback_).Run(true);

  payment_app_created_callback_.Run(std::move(app));
}

bool PaymentAppServiceBridge::SkipCreatingNativePaymentApps() const {
  return true;
}

void PaymentAppServiceBridge::OnPaymentAppCreationError(
    const std::string& error_message) {
  payment_app_creation_error_callback_.Run(error_message);
}

void PaymentAppServiceBridge::OnDoneCreatingPaymentApps() {
  if (number_of_pending_factories_ > 1U) {
    number_of_pending_factories_--;
    return;
  }

  DCHECK_EQ(1U, number_of_pending_factories_);

  if (can_make_payment_calculated_callback_)
    std::move(can_make_payment_calculated_callback_).Run(false);

  std::move(done_creating_payment_apps_callback_).Run();
  PaymentAppServiceBridgeStorage::GetInstance()->Remove(this);
}

void PaymentAppServiceBridge::SetCanMakePaymentEvenWithoutApps() {
  NOTREACHED();
}

}  // namespace payments
