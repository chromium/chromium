// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_context.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ContextualSearchContext_jni.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/browser/browser_thread.h"

ContextualSearchContext::ContextualSearchContext(JNIEnv* env, jobject obj) {
  java_object_.Reset(env, obj);
}

ContextualSearchContext::ContextualSearchContext(
    const std::string& home_country,
    const GURL& page_url,
    const std::string& encoding)
    : can_resolve_(true),
      can_send_base_page_url_(true),
      home_country_(home_country),
      base_page_url_(page_url),
      base_page_encoding_(encoding) {
  java_object_ = nullptr;
}

ContextualSearchContext::~ContextualSearchContext() {
}

// static
base::WeakPtr<ContextualSearchContext>
ContextualSearchContext::FromJavaContextualSearchContext(
    const base::android::JavaRef<jobject>& j_contextual_search_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (j_contextual_search_context.is_null())
    return NULL;

  ContextualSearchContext* contextual_search_context =
      reinterpret_cast<ContextualSearchContext*>(
          Java_ContextualSearchContext_getNativePointer(
              base::android::AttachCurrentThread(),
              j_contextual_search_context));
  return contextual_search_context->GetWeakPtr();
}

void ContextualSearchContext::SetResolveProperties(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& j_home_country,
    jboolean j_may_send_base_page_url,
    jlong j_previous_event_id,
    jint j_previous_event_results) {
  can_resolve_ = true;
  home_country_ = base::android::ConvertJavaStringToUTF8(env, j_home_country);
  can_send_base_page_url_ = j_may_send_base_page_url;
  previous_event_id_ = j_previous_event_id;
  previous_event_results_ = j_previous_event_results;
}

void ContextualSearchContext::SetContent(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& j_content,
    jint j_selection_start,
    jint j_selection_end) {
  SetSelectionSurroundings(
      j_selection_start, j_selection_end,
      base::android::ConvertJavaStringToUTF16(env, j_content));
}

void ContextualSearchContext::AdjustSelection(JNIEnv* env,
                                              jobject obj,
                                              jint j_start_adjust,
                                              jint j_end_adjust) {
  DCHECK(start_offset_ + j_start_adjust >= 0);
  DCHECK(start_offset_ + j_start_adjust <= (int)surrounding_text_.length());
  DCHECK(end_offset_ + j_end_adjust >= 0);
  DCHECK(end_offset_ + j_end_adjust <= (int)surrounding_text_.length());
  start_offset_ += j_start_adjust;
  end_offset_ += j_end_adjust;
}

// Accessors

bool ContextualSearchContext::CanResolve() const {
  return can_resolve_;
}

bool ContextualSearchContext::CanSendBasePageUrl() const {
  return can_send_base_page_url_;
}

const GURL ContextualSearchContext::GetBasePageUrl() const {
  return base_page_url_;
}

void ContextualSearchContext::SetBasePageUrl(const GURL& base_page_url) {
  this->base_page_url_ = base_page_url;
}

const std::string ContextualSearchContext::GetBasePageEncoding() const {
  return base_page_encoding_;
}

void ContextualSearchContext::SetBasePageEncoding(
    const std::string& base_page_encoding) {
  this->base_page_encoding_ = base_page_encoding;
}

const std::string ContextualSearchContext::GetHomeCountry() const {
  return home_country_;
}

int64_t ContextualSearchContext::GetPreviousEventId() const {
  return previous_event_id_;
}

int ContextualSearchContext::GetPreviousEventResults() const {
  return previous_event_results_;
}

void ContextualSearchContext::SetSelectionSurroundings(
    int start_offset,
    int end_offset,
    const std::u16string& surrounding_text) {
  this->start_offset_ = start_offset;
  this->end_offset_ = end_offset;
  this->surrounding_text_ = surrounding_text;
}

const std::u16string ContextualSearchContext::GetSurroundingText() const {
  return surrounding_text_;
}

int ContextualSearchContext::GetStartOffset() const {
  return start_offset_;
}

int ContextualSearchContext::GetEndOffset() const {
  return end_offset_;
}

void ContextualSearchContext::PrepareToResolve(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean j_is_exact_resolve,
    const base::android::JavaParamRef<jstring>& j_related_searches_stamp) {
  is_exact_resolve_ = j_is_exact_resolve;
  related_searches_stamp_ =
      base::android::ConvertJavaStringToUTF8(env, j_related_searches_stamp);
  do_related_searches_ = !related_searches_stamp_.empty();
}

bool ContextualSearchContext::GetExactResolve() const {
  return is_exact_resolve_;
}

base::android::ScopedJavaLocalRef<jstring>
ContextualSearchContext::DetectLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) const {
  std::string language = GetReliableLanguage(GetSelection());
  if (language.empty())
    language = GetReliableLanguage(this->surrounding_text_);
  base::android::ScopedJavaLocalRef<jstring> j_language =
      base::android::ConvertUTF8ToJavaString(env, language);
  return j_language;
}

void ContextualSearchContext::SetTranslationLanguages(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_detected_language,
    const base::android::JavaParamRef<jstring>& j_target_language,
    const base::android::JavaParamRef<jstring>& j_fluent_languages) {
  translation_languages_.detected_language =
      base::android::ConvertJavaStringToUTF8(env, j_detected_language);
  translation_languages_.target_language =
      base::android::ConvertJavaStringToUTF8(env, j_target_language);
  translation_languages_.fluent_languages =
      base::android::ConvertJavaStringToUTF8(env, j_fluent_languages);
}

const ContextualSearchContext::TranslationLanguages&
ContextualSearchContext::GetTranslationLanguages() const {
  return translation_languages_;
}

std::string ContextualSearchContext::GetReliableLanguage(
    const std::u16string& contents) const {
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score;
  std::string language = translate::DeterminePageLanguage(
      /*content_language=*/std::string(),
      /*html_lang=*/std::string(), contents, &model_detected_language,
      &is_model_reliable, model_reliability_score);
  // Make sure we return an empty string when unreliable or an unknown result.
  if (!is_model_reliable || language == translate::kUnknownLanguageCode)
    language = "";
  return language;
}

std::u16string ContextualSearchContext::GetSelection() const {
  int start = this->start_offset_;
  int end = this->end_offset_;
  DCHECK(start >= 0);
  DCHECK(end >= 0);
  DCHECK(end <= (int)this->surrounding_text_.length());
  DCHECK(start <= end);
  return this->surrounding_text_.substr(start, end - start);
}

bool ContextualSearchContext::GetRelatedSearches() const {
  return do_related_searches_;
}

const std::string ContextualSearchContext::GetRelatedSearchesStamp() const {
  return related_searches_stamp_;
}

// Boilerplate.

base::WeakPtr<ContextualSearchContext> ContextualSearchContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// Java wrapper boilerplate

void ContextualSearchContext::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

jlong JNI_ContextualSearchContext_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ContextualSearchContext* context = new ContextualSearchContext(env, obj);
  return reinterpret_cast<intptr_t>(context);
}
