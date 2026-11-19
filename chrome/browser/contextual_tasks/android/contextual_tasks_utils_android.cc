// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_bridge.h"
#include "chrome/browser/contextual_tasks/android/contextual_tasks_omnibox_query_helper.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextual_tasks/utils_jni_headers/ContextualTasksUtils_jni.h"

static bool JNI_ContextualTasksUtils_IsContextualTasksUiEnabled() {
  return contextual_tasks::IsContextualTasksUIEnabled();
}

static bool
JNI_ContextualTasksUtils_IsAllowSignedOutUserInDesktopAndroidEnabled() {
  return contextual_tasks::IsAllowSignedOutUserInDesktopAndroidEnabled();
}

static bool JNI_ContextualTasksUtils_ShouldShowAskGoogleEntryPoints(
    content::WebContents* web_contents) {
  return contextual_tasks::ContextualTasksBridge::
      ShouldShowAskGoogleEntryPoints(web_contents);
}

static void JNI_ContextualTasksUtils_OpenAskGoogleAboutThisPage(
    content::WebContents* web_contents,
    lens::LensOverlayInvocationSource invocation_source) {
  contextual_tasks::ContextualTasksBridge::OpenAskGoogleAboutThisPage(
      web_contents, invocation_source);
}

static void JNI_ContextualTasksUtils_ToggleAskGoogleSidePanel(
    content::WebContents* web_contents,
    lens::LensOverlayInvocationSource invocation_source) {
  contextual_tasks::ContextualTasksBridge::ToggleAskGoogleSidePanel(
      web_contents, invocation_source);
}

static void JNI_ContextualTasksUtils_IssueOmniboxContextualQuery(
    content::WebContents* web_contents,
    const GURL& suggestion_url) {
  contextual_tasks::ContextualTasksOmniboxQueryHelper::Start(web_contents,
                                                             suggestion_url);
}

DEFINE_JNI(ContextualTasksUtils)
