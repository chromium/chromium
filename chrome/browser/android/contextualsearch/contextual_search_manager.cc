// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_manager.h"

#include <memory>
#include <set>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/ContextualSearchManager_jni.h"
#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_search/content/browser/contextual_search_js_api_service_impl.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/url_request/url_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using content::WebContents;

namespace {

const char kContextualSearchObserverKey[] = "contextual_search_observer";

class ContextualSearchObserver : public content::WebContentsObserver,
                                 public base::SupportsUserData::Data {
 public:
  ContextualSearchObserver(
      content::WebContents* contents,
      contextual_search::ContextualSearchJsApiHandler* api_handler)
      : content::WebContentsObserver(contents) {
    registry_.AddInterface(base::Bind(
        &contextual_search::CreateContextualSearchJsApiService, api_handler));
  }
  ~ContextualSearchObserver() override = default;

  static void EnsureForWebContents(
      content::WebContents* contents,
      contextual_search::ContextualSearchJsApiHandler* api_handler) {
    // Clobber any prior registered observer.
    contents->SetUserData(
        kContextualSearchObserverKey,
        std::make_unique<ContextualSearchObserver>(contents, api_handler));
  }

 private:
  // content::WebContentsObserver:
  void OnInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override {
    registry_.TryBindInterface(interface_name, interface_pipe);
  }

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchObserver);
};

}  // namespace

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
      base::Bind(&ContextualSearchManager::OnSearchTermResolutionResponse,
                 base::Unretained(this)),
      base::Bind(&ContextualSearchManager::OnTextSurroundingSelectionAvailable,
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

base::android::ScopedJavaLocalRef<jstring>
ContextualSearchManager::GetTargetLanguage(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  DCHECK(delegate_);
  std::string target_language(delegate_->GetTargetLanguage());
  base::android::ScopedJavaLocalRef<jstring> j_target_language =
      base::android::ConvertUTF8ToJavaString(env, target_language);
  return j_target_language;
}

base::android::ScopedJavaLocalRef<jstring>
ContextualSearchManager::GetAcceptLanguages(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  std::string accept_languages = delegate_->GetAcceptLanguages();
  base::android::ScopedJavaLocalRef<jstring> j_accept_languages =
      base::android::ConvertUTF8ToJavaString(env, accept_languages);
  return j_accept_languages;
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
  Java_ContextualSearchManager_onSearchTermResolutionResponse(
      env, java_manager_, resolved_search_term.is_invalid,
      resolved_search_term.response_code, j_search_term, j_display_text,
      j_alternate_term, j_mid, resolved_search_term.prevent_preload,
      resolved_search_term.selection_start_adjust,
      resolved_search_term.selection_end_adjust, j_context_language,
      j_thumbnail_url, j_caption, j_quick_action_uri,
      resolved_search_term.quick_action_category,
      resolved_search_term.logged_event_id, j_search_url_full,
      j_search_url_preload, resolved_search_term.coca_card_tag);
}

void ContextualSearchManager::OnTextSurroundingSelectionAvailable(
    const std::string& encoding,
    const base::string16& surrounding_text,
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
  ContextualSearchObserver::EnsureForWebContents(overlay_web_contents, this);
}

void ContextualSearchManager::WhitelistContextualSearchJsApiUrl(
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
