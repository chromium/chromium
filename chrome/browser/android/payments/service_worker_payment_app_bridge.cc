// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/service_worker_payment_app_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "jni/ServiceWorkerPaymentAppBridge_jni.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/origin.h"

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
using ::payments::mojom::BasicCardType;
using ::payments::mojom::CanMakePaymentEventData;
using ::payments::mojom::CanMakePaymentEventDataPtr;
using ::payments::mojom::PaymentCurrencyAmount;
using ::payments::mojom::PaymentDetailsModifier;
using ::payments::mojom::PaymentDetailsModifierPtr;
using ::payments::mojom::PaymentItem;
using ::payments::mojom::PaymentMethodData;
using ::payments::mojom::PaymentMethodDataPtr;
using ::payments::mojom::PaymentRequestEventData;
using ::payments::mojom::PaymentRequestEventDataPtr;

void OnGotAllPaymentApps(
    const JavaRef<jobject>& jweb_contents,
    const JavaRef<jobject>& jcallback,
    content::PaymentAppProvider::PaymentApps apps,
    payments::ServiceWorkerPaymentAppFactory::InstallablePaymentApps
        installable_apps) {
  JNIEnv* env = AttachCurrentThread();

  for (const auto& app_info : apps) {
    // Sends related application Ids to java side if the app prefers related
    // applications.
    std::vector<std::string> preferred_related_application_ids;
    if (app_info.second->prefer_related_applications) {
      for (const auto& related_application :
           app_info.second->related_applications) {
        // Only consider related applications on Google play for Android.
        if (related_application.platform == "play")
          preferred_related_application_ids.emplace_back(
              related_application.id);
      }
    }

    base::android::ScopedJavaLocalRef<jobjectArray> jcapabilities =
        Java_ServiceWorkerPaymentAppBridge_createCapabilities(
            env, app_info.second->capabilities.size());
    for (size_t i = 0; i < app_info.second->capabilities.size(); i++) {
      Java_ServiceWorkerPaymentAppBridge_addCapabilities(
          env, jcapabilities, base::checked_cast<int>(i),
          ToJavaIntArray(
              env, app_info.second->capabilities[i].supported_card_networks),
          ToJavaIntArray(
              env, app_info.second->capabilities[i].supported_card_types));
    }

    // TODO(crbug.com/846077): Find a proper way to make use of user hint.
    Java_ServiceWorkerPaymentAppBridge_onPaymentAppCreated(
        env, app_info.second->registration_id,
        ConvertUTF8ToJavaString(env, app_info.second->scope.spec()),
        app_info.second->name.empty()
            ? nullptr
            : ConvertUTF8ToJavaString(env, app_info.second->name),
        nullptr, ConvertUTF8ToJavaString(env, app_info.second->scope.host()),
        app_info.second->icon == nullptr
            ? nullptr
            : gfx::ConvertToJavaBitmap(app_info.second->icon.get()),
        ToJavaArrayOfStrings(env, app_info.second->enabled_methods),
        app_info.second->has_explicitly_verified_methods, jcapabilities,
        ToJavaArrayOfStrings(env, preferred_related_application_ids),
        jweb_contents, jcallback);
  }

  for (const auto& installable_app : installable_apps) {
    Java_ServiceWorkerPaymentAppBridge_onInstallablePaymentAppCreated(
        env, ConvertUTF8ToJavaString(env, installable_app.second->name),
        ConvertUTF8ToJavaString(env, installable_app.second->sw_js_url),
        ConvertUTF8ToJavaString(env, installable_app.second->sw_scope),
        installable_app.second->sw_use_cache,
        installable_app.second->icon == nullptr
            ? nullptr
            : gfx::ConvertToJavaBitmap(installable_app.second->icon.get()),
        ConvertUTF8ToJavaString(env, installable_app.first.spec()),
        jweb_contents, jcallback);
  }

  Java_ServiceWorkerPaymentAppBridge_onAllPaymentAppsCreated(env, jcallback);
}

void OnHasServiceWorkerPaymentAppsResponse(
    const JavaRef<jobject>& jcallback,
    content::PaymentAppProvider::PaymentApps apps) {
  JNIEnv* env = AttachCurrentThread();

  Java_ServiceWorkerPaymentAppBridge_onHasServiceWorkerPaymentApps(
      env, jcallback, apps.size() > 0);
}

void OnGetServiceWorkerPaymentAppsInfo(
    const JavaRef<jobject>& jcallback,
    content::PaymentAppProvider::PaymentApps apps) {
  JNIEnv* env = AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> jappsInfo =
      Java_ServiceWorkerPaymentAppBridge_createPaymentAppsInfo(env);

  for (const auto& app_info : apps) {
    Java_ServiceWorkerPaymentAppBridge_addPaymentAppInfo(
        env, jappsInfo,
        ConvertUTF8ToJavaString(env, app_info.second->scope.host()),
        ConvertUTF8ToJavaString(env, app_info.second->name),
        app_info.second->icon == nullptr
            ? nullptr
            : gfx::ConvertToJavaBitmap(app_info.second->icon.get()));
  }

  Java_ServiceWorkerPaymentAppBridge_onGetServiceWorkerPaymentAppsInfo(
      env, jcallback, jappsInfo);
}

void OnCanMakePayment(const JavaRef<jobject>& jweb_contents,
                      const JavaRef<jobject>& jcallback,
                      bool can_make_payment) {
  JNIEnv* env = AttachCurrentThread();
  Java_ServiceWorkerPaymentAppBridge_onCanMakePayment(env, jcallback,
                                                      can_make_payment);
}

void OnPaymentAppInvoked(
    const JavaRef<jobject>& jweb_contents,
    const JavaRef<jobject>& jcallback,
    payments::mojom::PaymentHandlerResponsePtr handler_response) {
  JNIEnv* env = AttachCurrentThread();

  Java_ServiceWorkerPaymentAppBridge_onPaymentAppInvoked(
      env, jcallback,
      ConvertUTF8ToJavaString(env, handler_response->method_name),
      ConvertUTF8ToJavaString(env, handler_response->stringified_details));
}

void OnPaymentAppAborted(const JavaRef<jobject>& jweb_contents,
                         const JavaRef<jobject>& jcallback,
                         bool result) {
  JNIEnv* env = AttachCurrentThread();

  Java_ServiceWorkerPaymentAppBridge_onPaymentAppAborted(env, jcallback,
                                                         result);
}

template <typename T>
void ConvertIntsToEnums(const std::vector<int> ints, std::vector<T>* enums) {
  enums->resize(ints.size());
  for (size_t i = 0; i < ints.size(); ++i) {
    enums->at(i) = static_cast<T>(ints.at(i));
  }
}

std::vector<PaymentMethodDataPtr> ConvertPaymentMethodDataFromJavaToNative(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jmethod_data) {
  std::vector<PaymentMethodDataPtr> result;
  for (jsize i = 0; i < env->GetArrayLength(jmethod_data); i++) {
    ScopedJavaLocalRef<jobject> element(
        env, env->GetObjectArrayElement(jmethod_data, i));
    PaymentMethodDataPtr method_data_item = PaymentMethodData::New();
    method_data_item->supported_method = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedMethodFromMethodData(
            env, element));

    std::vector<int> supported_network_ints;
    base::android::JavaIntArrayToIntVector(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedNetworksFromMethodData(
            env, element),
        &supported_network_ints);
    ConvertIntsToEnums<BasicCardNetwork>(supported_network_ints,
                                         &method_data_item->supported_networks);

    std::vector<int> supported_type_ints;
    base::android::JavaIntArrayToIntVector(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedTypesFromMethodData(
            env, element),
        &supported_type_ints);
    ConvertIntsToEnums<BasicCardType>(supported_type_ints,
                                      &method_data_item->supported_types);

    method_data_item->stringified_data = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getStringifiedDataFromMethodData(
            env, element));
    result.push_back(std::move(method_data_item));
  }
  return result;
}

PaymentRequestEventDataPtr ConvertPaymentRequestEventDataFromJavaToNative(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobject>& jtotal,
    const JavaParamRef<jobjectArray>& jmodifiers) {
  PaymentRequestEventDataPtr event_data = PaymentRequestEventData::New();

  event_data->top_origin = GURL(ConvertJavaStringToUTF8(env, jtop_origin));
  event_data->payment_request_origin =
      GURL(ConvertJavaStringToUTF8(env, jpayment_request_origin));
  event_data->payment_request_id =
      ConvertJavaStringToUTF8(env, jpayment_request_id);
  event_data->method_data =
      ConvertPaymentMethodDataFromJavaToNative(env, jmethod_data);

  event_data->total = PaymentCurrencyAmount::New();
  event_data->total->currency = ConvertJavaStringToUTF8(
      env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
               env, jtotal));
  event_data->total->value = ConvertJavaStringToUTF8(
      env,
      Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentItem(env, jtotal));

  for (jsize i = 0; i < env->GetArrayLength(jmodifiers); i++) {
    ScopedJavaLocalRef<jobject> jmodifier(
        env, env->GetObjectArrayElement(jmodifiers, i));
    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();

    ScopedJavaLocalRef<jobject> jmodifier_total =
        Java_ServiceWorkerPaymentAppBridge_getTotalFromModifier(env, jmodifier);
    modifier->total = PaymentItem::New();
    modifier->total->label = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getLabelFromPaymentItem(
                 env, jmodifier_total));
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
                 env, jmodifier_total));
    modifier->total->amount->value = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentItem(
                 env, jmodifier_total));

    ScopedJavaLocalRef<jobject> jmodifier_method_data =
        Java_ServiceWorkerPaymentAppBridge_getMethodDataFromModifier(env,
                                                                     jmodifier);
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_method = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedMethodFromMethodData(
            env, jmodifier_method_data));
    modifier->method_data->stringified_data = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getStringifiedDataFromMethodData(
            env, jmodifier_method_data));

    event_data->modifiers.push_back(std::move(modifier));
  }

  return event_data;
}

}  // namespace

static void JNI_ServiceWorkerPaymentAppBridge_GetAllPaymentApps(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobjectArray>& jmethod_data,
    jboolean jmay_crawl_for_installable_payment_apps,
    const JavaParamRef<jobject>& jcallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  payments::ServiceWorkerPaymentAppFactory::GetInstance()->GetAllPaymentApps(
      web_contents,
      WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS),
      ConvertPaymentMethodDataFromJavaToNative(env, jmethod_data),
      jmay_crawl_for_installable_payment_apps,
      base::BindOnce(&OnGotAllPaymentApps,
                     ScopedJavaGlobalRef<jobject>(env, jweb_contents),
                     ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindOnce([]() {
        /* Nothing needs to be done after writing cache. This callback is used
         * only in tests. */
      }));
}

static void JNI_ServiceWorkerPaymentAppBridge_HasServiceWorkerPaymentApps(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jcallback) {
  // Checks whether there is a installed service worker payment app through
  // GetAllPaymentApps.
  content::PaymentAppProvider::GetInstance()->GetAllPaymentApps(
      ProfileManager::GetActiveUserProfile(),
      base::BindOnce(&OnHasServiceWorkerPaymentAppsResponse,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_GetServiceWorkerPaymentAppsInfo(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jcallback) {
  content::PaymentAppProvider::GetInstance()->GetAllPaymentApps(
      ProfileManager::GetActiveUserProfile(),
      base::BindOnce(&OnGetServiceWorkerPaymentAppsInfo,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_CanMakePayment(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents,
    jlong registration_id,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobjectArray>& jmodifiers,
    const JavaParamRef<jobject>& jcallback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  CanMakePaymentEventDataPtr event_data = CanMakePaymentEventData::New();

  event_data->top_origin = GURL(ConvertJavaStringToUTF8(env, jtop_origin));
  event_data->payment_request_origin =
      GURL(ConvertJavaStringToUTF8(env, jpayment_request_origin));
  event_data->method_data =
      ConvertPaymentMethodDataFromJavaToNative(env, jmethod_data);

  for (jsize i = 0; i < env->GetArrayLength(jmodifiers); i++) {
    ScopedJavaLocalRef<jobject> jmodifier(
        env, env->GetObjectArrayElement(jmodifiers, i));
    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();

    ScopedJavaLocalRef<jobject> jmodifier_total =
        Java_ServiceWorkerPaymentAppBridge_getTotalFromModifier(env, jmodifier);
    modifier->total = PaymentItem::New();
    modifier->total->label = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getLabelFromPaymentItem(
                 env, jmodifier_total));
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
                 env, jmodifier_total));
    modifier->total->amount->value = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentItem(
                 env, jmodifier_total));

    ScopedJavaLocalRef<jobject> jmodifier_method_data =
        Java_ServiceWorkerPaymentAppBridge_getMethodDataFromModifier(env,
                                                                     jmodifier);
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_method = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedMethodFromMethodData(
            env, jmodifier_method_data));
    modifier->method_data->stringified_data = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getStringifiedDataFromMethodData(
            env, jmodifier_method_data));

    event_data->modifiers.push_back(std::move(modifier));
  }

  content::PaymentAppProvider::GetInstance()->CanMakePayment(
      web_contents->GetBrowserContext(), registration_id, std::move(event_data),
      base::BindOnce(&OnCanMakePayment,
                     ScopedJavaGlobalRef<jobject>(env, jweb_contents),
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_InvokePaymentApp(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents,
    jlong registration_id,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobject>& jtotal,
    const JavaParamRef<jobjectArray>& jmodifiers,
    const JavaParamRef<jobject>& jcallback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  content::PaymentAppProvider::GetInstance()->InvokePaymentApp(
      web_contents->GetBrowserContext(), registration_id,
      ConvertPaymentRequestEventDataFromJavaToNative(
          env, jtop_origin, jpayment_request_origin, jpayment_request_id,
          jmethod_data, jtotal, jmodifiers),
      base::BindOnce(&OnPaymentAppInvoked,
                     ScopedJavaGlobalRef<jobject>(env, jweb_contents),
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_InstallAndInvokePaymentApp(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobject>& jtotal,
    const JavaParamRef<jobjectArray>& jmodifiers,
    const JavaParamRef<jobject>& jcallback,
    const JavaParamRef<jstring>& japp_name,
    const JavaParamRef<jobject>& jicon,
    const JavaParamRef<jstring>& jsw_js_url,
    const JavaParamRef<jstring>& jsw_scope,
    jboolean juse_cache,
    const JavaParamRef<jstring>& jmethod) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  SkBitmap icon_bitmap;
  if (jicon) {
    icon_bitmap = gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(jicon));
  }

  content::PaymentAppProvider::GetInstance()->InstallAndInvokePaymentApp(
      web_contents,
      ConvertPaymentRequestEventDataFromJavaToNative(
          env, jtop_origin, jpayment_request_origin, jpayment_request_id,
          jmethod_data, jtotal, jmodifiers),
      ConvertJavaStringToUTF8(env, japp_name), icon_bitmap,
      ConvertJavaStringToUTF8(env, jsw_js_url),
      ConvertJavaStringToUTF8(env, jsw_scope), juse_cache,
      ConvertJavaStringToUTF8(env, jmethod),
      base::BindOnce(&OnPaymentAppInvoked,
                     ScopedJavaGlobalRef<jobject>(env, jweb_contents),
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_AbortPaymentApp(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents,
    jlong registration_id,
    const JavaParamRef<jobject>& jcallback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  content::PaymentAppProvider::GetInstance()->AbortPayment(
      web_contents->GetBrowserContext(), registration_id,
      base::BindOnce(&OnPaymentAppAborted,
                     ScopedJavaGlobalRef<jobject>(env, jweb_contents),
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_OnClosingPaymentAppWindow(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  content::PaymentAppProvider::GetInstance()->OnClosingOpenedWindow(
      web_contents->GetBrowserContext());
}
