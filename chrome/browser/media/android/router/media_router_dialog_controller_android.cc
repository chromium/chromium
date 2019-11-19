// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/router/media_router_dialog_controller_android.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/features/media_router/jni_headers/ChromeMediaRouterDialogController_jni.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/media/android/router/media_router_android.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/media_router/media_source.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "device/vr/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

namespace media_router {

// static
MediaRouterDialogController*
MediaRouterDialogController::GetOrCreateForWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);
  // This call does nothing if the controller already exists.
  MediaRouterDialogControllerAndroid::CreateForWebContents(web_contents);
  return MediaRouterDialogControllerAndroid::FromWebContents(web_contents);
}

void MediaRouterDialogControllerAndroid::OnSinkSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jsource_id,
    const JavaParamRef<jstring>& jsink_id) {
  auto start_presentation_context = std::move(start_presentation_context_);
  if (!start_presentation_context)
    return;

  const auto& presentation_request =
      start_presentation_context->presentation_request();

  const MediaSource::Id source_id = ConvertJavaStringToUTF8(env, jsource_id);

#ifndef NDEBUG
  // Verify that there was a request containing the source id the sink was
  // selected for.
  std::vector<MediaSource> sources;
  for (const auto& url : presentation_request.presentation_urls)
    sources.push_back(MediaSource::ForPresentationUrl(url));
  bool is_source_from_request = false;
  for (const auto& source : sources) {
    if (source.id() == source_id) {
      is_source_from_request = true;
      break;
    }
  }
  DCHECK(is_source_from_request);
#endif  // NDEBUG

  content::BrowserContext* browser_context = initiator()->GetBrowserContext();
  MediaRouter* router =
      MediaRouterFactory::GetApiForBrowserContext(browser_context);
  router->CreateRoute(
      source_id, ConvertJavaStringToUTF8(env, jsink_id),
      presentation_request.frame_origin, initiator(),
      base::BindOnce(&StartPresentationContext::HandleRouteResponse,
                     std::move(start_presentation_context)),
      base::TimeDelta(), browser_context->IsOffTheRecord());
}

void MediaRouterDialogControllerAndroid::OnRouteClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jmedia_route_id) {
  std::string media_route_id = ConvertJavaStringToUTF8(env, jmedia_route_id);

  MediaRouter* router = MediaRouterFactory::GetApiForBrowserContext(
      initiator()->GetBrowserContext());

  router->TerminateRoute(media_route_id);

  CancelPresentationRequest();
}

void MediaRouterDialogControllerAndroid::OnDialogCancelled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CancelPresentationRequest();
}

void MediaRouterDialogControllerAndroid::OnMediaSourceNotSupported(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  auto request = std::move(start_presentation_context_);
  if (!request)
    return;

  request->InvokeErrorCallback(blink::mojom::PresentationError(
      blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
      "No screens found."));
}

void MediaRouterDialogControllerAndroid::CancelPresentationRequest() {
  auto request = std::move(start_presentation_context_);
  if (!request)
    return;

  request->InvokeErrorCallback(blink::mojom::PresentationError(
      blink::mojom::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED,
      "Dialog closed."));
}

MediaRouterDialogControllerAndroid::MediaRouterDialogControllerAndroid(
    WebContents* web_contents)
    : MediaRouterDialogController(web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_dialog_controller_.Reset(Java_ChromeMediaRouterDialogController_create(
      env, reinterpret_cast<jlong>(this)));
}

MediaRouterDialogControllerAndroid::~MediaRouterDialogControllerAndroid() {}

void MediaRouterDialogControllerAndroid::CreateMediaRouterDialog() {
  // TODO(crbug.com/736568): Re-enable dialog in VR.
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          initiator(),
          vr::UiSuppressedElement::kMediaRouterPresentationRequest)) {
    CancelPresentationRequest();
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<MediaSource> sources;
  for (const auto& url :
       start_presentation_context_->presentation_request().presentation_urls)
    sources.push_back(MediaSource::ForPresentationUrl(url));

  // If it's a single route with the same source, show the controller dialog
  // instead of the device picker.
  // TODO(avayvod): maybe this logic should be in
  // PresentationServiceDelegateImpl: if the route exists for the same frame
  // and tab, show the route controller dialog, if not, show the device picker.
  MediaRouterAndroid* router = static_cast<MediaRouterAndroid*>(
      MediaRouterFactory::GetApiForBrowserContext(
          initiator()->GetBrowserContext()));
  for (const auto& source : sources) {
    const MediaSource::Id& source_id = source.id();
    const MediaRoute* matching_route = router->FindRouteBySource(source_id);
    if (!matching_route)
      continue;

    ScopedJavaLocalRef<jstring> jsource_id =
        base::android::ConvertUTF8ToJavaString(env, source_id);
    ScopedJavaLocalRef<jstring> jmedia_route_id =
        base::android::ConvertUTF8ToJavaString(
            env, matching_route->media_route_id());

    Java_ChromeMediaRouterDialogController_openRouteControllerDialog(
        env, java_dialog_controller_, jsource_id, jmedia_route_id);
    return;
  }

  std::vector<base::string16> source_ids;
  source_ids.reserve(sources.size());
  for (const auto& source : sources)
    source_ids.push_back(base::UTF8ToUTF16(source.id()));
  ScopedJavaLocalRef<jobjectArray> jsource_ids =
      base::android::ToJavaArrayOfStrings(env, source_ids);
  Java_ChromeMediaRouterDialogController_openRouteChooserDialog(
      env, java_dialog_controller_, jsource_ids);
}

void MediaRouterDialogControllerAndroid::CloseMediaRouterDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_ChromeMediaRouterDialogController_closeDialog(env,
                                                     java_dialog_controller_);
}

bool MediaRouterDialogControllerAndroid::IsShowingMediaRouterDialog() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ChromeMediaRouterDialogController_isShowingDialog(
      env, java_dialog_controller_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaRouterDialogControllerAndroid)

}  // namespace media_router
