// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/payments/content/android/payment_handler_host.h"
#include "components/payments/content/payment_event_response_util.h"
#include "components/payments/content/payment_handler_host.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/installed_payment_apps_finder.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/payment_app_provider_util.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ServiceWorkerPaymentAppBridge_jni.h"

namespace {

using ::base::android::AppendJavaStringArrayToStringVector;
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;
using ::base::android::ToJavaArrayOfStrings;
using ::base::android::ToJavaIntArray;
using ::payments::mojom::BasicCardNetwork;
using ::payments::mojom::CanMakePaymentEventData;
using ::payments::mojom::CanMakePaymentEventDataPtr;
using ::payments::mojom::PaymentCurrencyAmount;
using ::payments::mojom::PaymentDetailsModifier;
using ::payments::mojom::PaymentDetailsModifierPtr;
using ::payments::mojom::PaymentItem;
using ::payments::mojom::PaymentMethodData;
using ::payments::mojom::PaymentMethodDataPtr;
using ::payments::mojom::PaymentOptions;
using ::payments::mojom::PaymentOptionsPtr;
using ::payments::mojom::PaymentRequestEventData;
using ::payments::mojom::PaymentRequestEventDataPtr;
using ::payments::mojom::PaymentShippingOption;
using ::payments::mojom::PaymentShippingOptionPtr;
using ::payments::mojom::PaymentShippingType;

void OnHasServiceWorkerPaymentAppsResponse(
    const JavaRef<jobject>& jcallback,
    content::InstalledPaymentAppsFinder::PaymentApps apps) {
  JNIEnv* env = AttachCurrentThread();

  Java_ServiceWorkerPaymentAppBridge_onHasServiceWorkerPaymentApps(
      env, jcallback, apps.size() > 0);
}

void OnGetServiceWorkerPaymentAppsInfo(
    const JavaRef<jobject>& jcallback,
    content::InstalledPaymentAppsFinder::PaymentApps apps) {
  JNIEnv* env = AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> jappsInfo =
      Java_ServiceWorkerPaymentAppBridge_createPaymentAppsInfo(env);

  for (const auto& app_info : apps) {
    Java_ServiceWorkerPaymentAppBridge_addPaymentAppInfo(
        env, jappsInfo, app_info.second->scope.host(), app_info.second->name,
        app_info.second->icon == nullptr
            ? nullptr
            : gfx::ConvertToJavaBitmap(*app_info.second->icon));
  }

  Java_ServiceWorkerPaymentAppBridge_onGetServiceWorkerPaymentAppsInfo(
      env, jcallback, jappsInfo);
}

}  // namespace

static void JNI_ServiceWorkerPaymentAppBridge_HasServiceWorkerPaymentApps(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  // Checks whether there is a installed service worker payment app through
  // GetAllPaymentApps.
  content::InstalledPaymentAppsFinder::GetInstance(
      ProfileManager::GetActiveUserProfile())
      ->GetAllPaymentApps(
          base::BindOnce(&OnHasServiceWorkerPaymentAppsResponse,
                         ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_GetServiceWorkerPaymentAppsInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  content::InstalledPaymentAppsFinder::GetInstance(
      ProfileManager::GetActiveUserProfile())
      ->GetAllPaymentApps(
          base::BindOnce(&OnGetServiceWorkerPaymentAppsInfo,
                         ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_OnClosingPaymentAppWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& payment_request_jweb_contents,
    jint reason) {
  content::WebContents* payment_request_web_contents =
      content::WebContents::FromJavaWebContents(payment_request_jweb_contents);
  DCHECK(payment_request_web_contents);  // Verified in Java before invoking
                                         // this function.
  content::PaymentAppProvider::GetOrCreateForWebContents(
      payment_request_web_contents)
      ->OnClosingOpenedWindow(
          static_cast<payments::mojom::PaymentEventResponseType>(reason));
}

static void JNI_ServiceWorkerPaymentAppBridge_OnOpeningPaymentAppWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& payment_request_jweb_contents,
    const JavaParamRef<jobject>& payment_handler_jweb_contents) {
  content::WebContents* payment_request_web_contents =
      content::WebContents::FromJavaWebContents(payment_request_jweb_contents);
  content::WebContents* payment_handler_web_contents =
      content::WebContents::FromJavaWebContents(payment_handler_jweb_contents);
  DCHECK(payment_request_web_contents);  // Verified in Java before invoking
                                         // this function.
  DCHECK(payment_handler_web_contents);  // Verified in Java before invoking
                                         // this function.
  content::PaymentAppProvider::GetOrCreateForWebContents(
      payment_request_web_contents)
      ->SetOpenedWindow(payment_handler_web_contents);
}

static jlong
JNI_ServiceWorkerPaymentAppBridge_GetSourceIdForPaymentAppFromScope(
    JNIEnv* env,
    const JavaParamRef<jobject>& jscope) {
  // At this point we know that the payment handler window is open for the
  // payment app associated with this scope. Since this getter is called inside
  // PaymentApp::getUkmSourceId() function which in turn gets called for the
  // invoked app inside
  // ChromePaymentRequestService::openPaymentHandlerWindowInternal.
  return content::PaymentAppProviderUtil::GetSourceIdForPaymentAppFromScope(
      url::GURLAndroid::ToNativeGURL(env, jscope).DeprecatedGetOriginAsURL());
}
