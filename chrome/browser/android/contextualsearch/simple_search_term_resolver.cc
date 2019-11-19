// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/simple_search_term_resolver.h"

#include <memory>
#include <set>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/SimpleSearchTermResolver_jni.h"
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

SimpleSearchTermResolver::SimpleSearchTermResolver(
    JNIEnv* env,
    const JavaRef<jobject>& obj) {
  java_instance_.Reset(obj);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  delegate_.reset(new ContextualSearchDelegate(
      profile->GetURLLoaderFactory(),
      TemplateURLServiceFactory::GetForProfile(profile),
      base::BindRepeating(
          &SimpleSearchTermResolver::OnSearchTermResolutionResponse,
          base::Unretained(this)),
      base::BindRepeating(
          &SimpleSearchTermResolver::OnTextSurroundingSelectionAvailable,
          base::Unretained(this))));
}

SimpleSearchTermResolver::~SimpleSearchTermResolver() {}

void SimpleSearchTermResolver::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

void SimpleSearchTermResolver::StartSearchTermResolutionRequest(
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
  delegate_->SetActiveContext(contextual_search_context);
  // Calls back to OnSearchTermResolutionResponse.
  delegate_->StartSearchTermResolutionRequest(contextual_search_context,
                                              base_web_contents);
}

void SimpleSearchTermResolver::OnSearchTermResolutionResponse(
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
  Java_SimpleSearchTermResolver_onSearchTermResolutionResponse(
      env, java_instance_, resolved_search_term.is_invalid,
      resolved_search_term.response_code, j_search_term, j_display_text,
      j_alternate_term, j_mid, resolved_search_term.prevent_preload,
      resolved_search_term.selection_start_adjust,
      resolved_search_term.selection_end_adjust, j_context_language,
      j_thumbnail_url, j_caption, j_quick_action_uri,
      resolved_search_term.quick_action_category,
      resolved_search_term.logged_event_id, j_search_url_full,
      j_search_url_preload, resolved_search_term.coca_card_tag);
}

void SimpleSearchTermResolver::OnTextSurroundingSelectionAvailable(
    const std::string& encoding,
    const base::string16& surrounding_text,
    size_t start_offset,
    size_t end_offset) {
  // For now we ignore the callback -- shouldn't happen anyway.
}

jlong JNI_SimpleSearchTermResolver_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  SimpleSearchTermResolver* instance = new SimpleSearchTermResolver(env, obj);
  return reinterpret_cast<intptr_t>(instance);
}
