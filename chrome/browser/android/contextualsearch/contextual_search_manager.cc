// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_manager.h"

#include <memory>
#include <set>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/ContextualSearchManager_jni.h"
#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"
#include "chrome/browser/android/contextualsearch/contextual_search_observer.h"
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_search/content/browser/contextual_search_js_api_service_impl.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/url_request/url_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using content::WebContents;

// This class manages the native behavior of the Contextual Search feature.
// Instances of this class are owned by the Java ContextualSearchManager.
// Most of the work is actually done in an associated delegate to this class:
// the ContextualSearchDelegate.
ContextualSearchManager::ContextualSearchManager(JNIEnv* env,
                                                 const JavaRef<jobject>& obj) {
  java_manager_.Reset(obj);
  Java_ContextualSearchManager_setNativeManager(
      env, obj, reinterpret_cast<intptr_t>(this));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  delegate_.reset(new ContextualSearchDelegate(
      profile->GetURLLoaderFactory(),
      TemplateURLServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &ContextualSearchManager::OnSearchTermResolutionResponse,
          base::Unretained(this)),
      base::BindRepeating(
          &ContextualSearchManager::OnTextSurroundingSelectionAvailable,
          base::Unretained(this))));
}

ContextualSearchManager::~ContextualSearchManager() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualSearchManager_clearNativeManager(env, java_manager_);
}

void ContextualSearchManager::Destroy(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  delete this;
}

void ContextualSearchManager::StartSearchTermResolutionRequest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_contextual_search_context,
    const JavaParamRef<jobject>& j_base_web_contents) {
  WebContents* base_web_contents =
      WebContents::FromJavaWebContents(j_base_web_contents);
  DCHECK(base_web_contents);
  base::WeakPtr<ContextualSearchContext> contextual_search_context =
      ContextualSearchContext::FromJavaContextualSearchContext(
          j_contextual_search_context);
  // Calls back to OnSearchTermResolutionResponse.
  delegate_->StartSearchTermResolutionRequest(contextual_search_context,
                                              base_web_contents);
}

void ContextualSearchManager::GatherSurroundingText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_contextual_search_context,
    const JavaParamRef<jobject>& j_base_web_contents) {
  WebContents* base_web_contents =
      WebContents::FromJavaWebContents(j_base_web_contents);
  DCHECK(base_web_contents);
  base::WeakPtr<ContextualSearchContext> contextual_search_context =
      ContextualSearchContext::FromJavaContextualSearchContext(
          j_contextual_search_context);
  delegate_->GatherAndSaveSurroundingText(contextual_search_context,
                                          base_web_contents);
}

void ContextualSearchManager::OnSearchTermResolutionResponse(
    const ResolvedSearchTerm& resolved_search_term) {
  // Notify the Java UX of the result.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_search_term =
      base::android::ConvertUTF8ToJavaString(env,
                                             resolved_search_term.search_term);
  base::android::ScopedJavaLocalRef<jstring> j_display_text =
      base::android::ConvertUTF8ToJavaString(env,
                                             resolved_search_term.display_text);
  base::android::ScopedJavaLocalRef<jstring> j_alternate_term =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.alternate_term);
  base::android::ScopedJavaLocalRef<jstring> j_mid =
      base::android::ConvertUTF8ToJavaString(env, resolved_search_term.mid);
  base::android::ScopedJavaLocalRef<jstring> j_context_language =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.context_language);
  base::android::ScopedJavaLocalRef<jstring> j_thumbnail_url =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.thumbnail_url);
  base::android::ScopedJavaLocalRef<jstring> j_caption =
      base::android::ConvertUTF8ToJavaString(env, resolved_search_term.caption);
  base::android::ScopedJavaLocalRef<jstring> j_quick_action_uri =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.quick_action_uri);
  base::android::ScopedJavaLocalRef<jstring> j_search_url_full =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.search_url_full);
  base::android::ScopedJavaLocalRef<jstring> j_search_url_preload =
      base::android::ConvertUTF8ToJavaString(
          env, resolved_search_term.search_url_preload);
  base::android::ScopedJavaLocalRef<jobjectArray> j_searches =
      base::android::ToJavaArrayOfStrings(
          env, resolved_search_term.related_searches);
  Java_ContextualSearchManager_onSearchTermResolutionResponse(
      env, java_manager_, resolved_search_term.is_invalid,
      resolved_search_term.response_code, j_search_term, j_display_text,
      j_alternate_term, j_mid, resolved_search_term.prevent_preload,
      resolved_search_term.selection_start_adjust,
      resolved_search_term.selection_end_adjust, j_context_language,
      j_thumbnail_url, j_caption, j_quick_action_uri,
      resolved_search_term.quick_action_category,
      resolved_search_term.logged_event_id, j_search_url_full,
      j_search_url_preload, resolved_search_term.coca_card_tag, j_searches);
}

void ContextualSearchManager::OnTextSurroundingSelectionAvailable(
    const std::string& encoding,
    const std::u16string& surrounding_text,
    size_t start_offset,
    size_t end_offset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_encoding =
      base::android::ConvertUTF8ToJavaString(env, encoding.c_str());
  base::android::ScopedJavaLocalRef<jstring> j_surrounding_text =
      base::android::ConvertUTF16ToJavaString(env, surrounding_text.c_str());
  Java_ContextualSearchManager_onTextSurroundingSelectionAvailable(
      env, java_manager_, j_encoding, j_surrounding_text, start_offset,
      end_offset);
}

void ContextualSearchManager::EnableContextualSearchJsApiForWebContents(
    JNIEnv* env,
    jobject obj,
    const JavaParamRef<jobject>& j_overlay_web_contents) {
  DCHECK(j_overlay_web_contents);
  WebContents* overlay_web_contents =
      WebContents::FromJavaWebContents(j_overlay_web_contents);
  DCHECK(overlay_web_contents);

  // It's safe to use a raw pointer since the lifetime of |this| matches the
  // application lifetime, and therefore spans multiple WebContents.
  contextual_search::ContextualSearchObserver::SetHandlerForWebContents(
      overlay_web_contents, this);
}

void ContextualSearchManager::AllowlistContextualSearchJsApiUrl(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& j_url) {
  DCHECK(j_url);
  overlay_gurl_ = GURL(base::android::ConvertJavaStringToUTF8(env, j_url));
}

void ContextualSearchManager::ShouldEnableJsApi(
    const GURL& gurl,
    contextual_search::mojom::ContextualSearchJsApiService::
        ShouldEnableJsApiCallback callback) {
  bool should_enable = (gurl == overlay_gurl_);
  std::move(callback).Run(should_enable);
}

void ContextualSearchManager::SetCaption(const std::string& caption,
                                         bool does_answer) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_caption =
      base::android::ConvertUTF8ToJavaString(env, caption.c_str());
  Java_ContextualSearchManager_onSetCaption(env, java_manager_, j_caption,
                                            does_answer);
}

void ContextualSearchManager::ChangeOverlayPosition(
    contextual_search::mojom::OverlayPosition desired_position) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualSearchManager_onChangeOverlayPosition(
      env, java_manager_, static_cast<int>(desired_position));
}

jlong JNI_ContextualSearchManager_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  ContextualSearchManager* manager = new ContextualSearchManager(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}
