// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "chrome/android/chrome_jni_headers/HistoricalTabSaver_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;

namespace {

void CreateHistoricalTab(content::WebContents* web_contents) {
  DCHECK(web_contents);

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!service)
    return;

  // Exclude internal pages from being marked as recent when they are closed.
  const GURL& tab_url = web_contents->GetURL();
  if (tab_url.SchemeIs(content::kChromeUIScheme) ||
      tab_url.SchemeIs(chrome::kChromeNativeScheme) ||
      tab_url.SchemeIs(url::kAboutScheme)) {
    return;
  }

  // TODO(jcivelli): is the index important?
  service->CreateHistoricalTab(
      sessions::ContentLiveTab::GetForWebContents(web_contents), -1);
}
}  // anonymous namespace

// Static JNI methods.

// static
static void JNI_HistoricalTabSaver_CreateHistoricalTabFromContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (web_contents)
    CreateHistoricalTab(web_contents);
}
