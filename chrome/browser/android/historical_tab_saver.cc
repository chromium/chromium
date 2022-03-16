// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/historical_tab_saver.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/HistoricalTabSaver_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"

using base::android::JavaParamRef;

namespace historical_tab_saver {

namespace {

void CreateHistoricalTab(TabAndroid* tab_android) {
  if (!tab_android) {
    return;
  }

  auto scoped_web_contents = ScopedWebContents::CreateForTab(tab_android);
  if (!scoped_web_contents->web_contents()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(Profile::FromBrowserContext(
          scoped_web_contents->web_contents()->GetBrowserContext()));
  if (!service) {
    return;
  }

  // Index is unimportant on Android.
  service->CreateHistoricalTab(sessions::ContentLiveTab::GetForWebContents(
                                   scoped_web_contents->web_contents()),
                               /*index=*/-1);
}

}  // namespace

ScopedWebContents::ScopedWebContents(content::WebContents* web_contents,
                                     bool was_frozen)
    : web_contents_(web_contents), was_frozen_(was_frozen) {}

ScopedWebContents::~ScopedWebContents() {
  if (was_frozen_ && web_contents_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_HistoricalTabSaver_destroyTemporaryWebContents(
        env, web_contents_->GetJavaWebContents());
  }
}

// static
std::unique_ptr<ScopedWebContents> ScopedWebContents::CreateForTab(
    TabAndroid* tab) {
  bool was_frozen = false;
  content::WebContents* contents = tab->web_contents();
  if (!contents) {
    JNIEnv* env = base::android::AttachCurrentThread();
    contents = content::WebContents::FromJavaWebContents(
        Java_HistoricalTabSaver_createTemporaryWebContents(
            env, tab->GetJavaObject()));
    was_frozen = true;
  }
  return base::WrapUnique(new ScopedWebContents(contents, was_frozen));
}

// Static JNI methods.

// static
static void JNI_HistoricalTabSaver_CreateHistoricalTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& jtab_android) {
  CreateHistoricalTab(TabAndroid::GetNativeTab(env, jtab_android));
}

}  // namespace historical_tab_saver
