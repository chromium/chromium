// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/location_bar_model_util.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextual_tasks/utils_jni_headers/ContextualTasksUtils_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

static GURL JNI_ContextualTasksUtils_GetContextualTasksDisplayUrl(
    JNIEnv* env,
    content::WebContents* web_contents) {
  return contextual_tasks::GetContextualTasksDisplayURL(web_contents);
}

static GURL JNI_ContextualTasksUtils_GetContextualTasksFunctionalURL(
    JNIEnv* env,
    content::WebContents* web_contents) {
  return contextual_tasks::GetContextualTasksFunctionalURL(web_contents);
}

static ScopedJavaLocalRef<jstring> JNI_ContextualTasksUtils_GetReplacementUrl(
    JNIEnv* env,
    const std::u16string& current_text,
    int32_t selection_start,
    int32_t selection_end,
    const GURL& functional_gurl) {
  if (selection_start < 0 ||
      selection_end > static_cast<int>(current_text.length()) ||
      selection_start >= selection_end) {
    return nullptr;
  }

  // Do not adjust if selection did not start at the beginning of the field.
  if (selection_start != 0) {
    return nullptr;
  }

  std::string selected_text_utf8 = base::UTF16ToUTF8(
      current_text.substr(selection_start, selection_end - selection_start));

  GURL url_from_text(selected_text_utf8);

  GURL replacement_url = location_bar_model::AdjustContextualTasksURLForCopy(
      url_from_text, functional_gurl);

  if (replacement_url.is_valid()) {
    return ConvertUTF8ToJavaString(env, replacement_url.spec());
  }

  return nullptr;
}

DEFINE_JNI(ContextualTasksUtils)
