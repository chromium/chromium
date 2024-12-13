// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_manager.h"

#include <memory>
#include <set>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/android/contextualsearch/native_contextual_search_context.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/contextual_search/core/browser/contextual_search_delegate_impl.h"
#include "components/contextual_search/core/browser/resolved_search_term.h"
#include "components/history/core/browser/history_service.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ContextualSearchManager_jni.h"
#include "chrome/android/chrome_jni_headers/ContextualSearchPolicy_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using content::WebContents;

namespace {
const int kHistoryDeletionWindowSeconds = 2;
}  // namespace

// This class manages the native behavior of the Contextual Search feature.
// Instances of this class are owned by the Java ContextualSearchManager.
// Most of the work is actually done in an associated delegate to this class:
// the ContextualSearchDelegate.
ContextualSearchManager::ContextualSearchManager(JNIEnv* env,
                                                 const JavaRef<jobject>& obj,
                                                 Profile* profile)
    : profile_(profile) {
  java_manager_.Reset(obj);
  Java_ContextualSearchManager_setNativeManager(
      env, obj, reinterpret_cast<intptr_t>(this));
  delegate_ = std::make_unique<ContextualSearchDelegateImpl>(
      profile->GetURLLoaderFactory(),
      TemplateURLServiceFactory::GetForProfile(profile));
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
  base::WeakPtr<NativeContextualSearchContext> contextual_search_context =
      NativeContextualSearchContext::FromJavaContextualSearchContext(
          j_contextual_search_context);
  // Calls back to OnSearchTermResolutionResponse.
  delegate_->StartSearchTermResolutionRequest(
      contextual_search_context, base_web_contents,
      base::BindRepeating(
          &ContextualSearchManager::OnSearchTermResolutionResponse,
          base::Unretained(this)));
}

void ContextualSearchManager::GatherSurroundingText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& j_contextual_search_context,
    const JavaParamRef<jobject>& j_base_web_contents) {
  WebContents* base_web_contents =
      WebContents::FromJavaWebContents(j_base_web_contents);
  DCHECK(base_web_contents);
  base::WeakPtr<NativeContextualSearchContext> contextual_search_context =
      NativeContextualSearchContext::FromJavaContextualSearchContext(
          j_contextual_search_context);
  delegate_->GatherAndSaveSurroundingText(
      contextual_search_context, base_web_contents,
      base::BindRepeating(
          &ContextualSearchManager::OnTextSurroundingSelectionAvailable,
          base::Unretained(this)));
}

void ContextualSearchManager::RemoveLastHistoryEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    std::string& search_url,
    jlong search_start_time_ms) {
  // The deletion window is from the time a search URL was put in history, up
  // to a short amount of time later.
  base::Time begin_time =
      base::Time::FromMillisecondsSinceUnixEpoch(search_start_time_ms);
  base::Time end_time =
      begin_time + base::Seconds(kHistoryDeletionWindowSeconds);

  history::HistoryService* service = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (service) {
    // NOTE(mathp): We are only removing |search_url| from the local history
    // because search results that are not promoted to a Tab do not make it to
    // the web history, only local.
    std::set<GURL> restrict_set;
    restrict_set.insert(GURL(search_url));
    service->ExpireHistoryBetween(
        restrict_set, history::kNoAppIdFilter, begin_time, end_time,
        /*user_initiated*/ false, base::DoNothing(), &history_task_tracker_);
  }
}

void ContextualSearchManager::OnSearchTermResolutionResponse(
    const ResolvedSearchTerm& resolved_search_term) {
  // Notify the Java UX of the result.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualSearchManager_onSearchTermResolutionResponse(
      env, java_manager_, resolved_search_term.is_invalid,
      resolved_search_term.response_code, resolved_search_term.search_term,
      resolved_search_term.display_text, resolved_search_term.alternate_term,
      resolved_search_term.mid, resolved_search_term.prevent_preload,
      resolved_search_term.selection_start_adjust,
      resolved_search_term.selection_end_adjust,
      resolved_search_term.context_language, resolved_search_term.thumbnail_url,
      resolved_search_term.caption, resolved_search_term.quick_action_uri,
      resolved_search_term.quick_action_category,
      resolved_search_term.search_url_full,
      resolved_search_term.search_url_preload,
      resolved_search_term.coca_card_tag,
      resolved_search_term.related_searches_json);
}

void ContextualSearchManager::OnTextSurroundingSelectionAvailable(
    const std::string& encoding,
    const std::u16string& surrounding_text,
    size_t start_offset,
    size_t end_offset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualSearchManager_onTextSurroundingSelectionAvailable(
      env, java_manager_, encoding, surrounding_text, start_offset, end_offset);
}

jlong JNI_ContextualSearchManager_Init(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       Profile* profile) {
  ContextualSearchManager* manager =
      new ContextualSearchManager(env, obj, profile);
  return reinterpret_cast<intptr_t>(manager);
}

jboolean JNI_ContextualSearchPolicy_IsContextualSearchResolutionUrlValid(
    JNIEnv* env,
    Profile* profile) {
  // Attempt to resolve a (empty) query. Return whether resulting URL is
  // navigable.
  auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return false;
  }

  auto* template_url = template_url_service->GetDefaultSearchProvider();
  if (!template_url) {
    return false;
  }

  auto contextual_search_url_ref = template_url->contextual_search_url_ref();

  GURL url(contextual_search_url_ref.ReplaceSearchTerms(
      {}, template_url_service->search_terms_data(), NULL));

  return url.is_valid();
}
