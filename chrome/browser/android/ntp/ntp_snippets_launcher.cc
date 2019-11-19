// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_launcher.h"

#include "chrome/android/chrome_jni_headers/SnippetsLauncher_jni.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

base::LazyInstance<NTPSnippetsLauncher>::DestructorAtExit g_snippets_launcher =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
NTPSnippetsLauncher* NTPSnippetsLauncher::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_snippets_launcher.Pointer();
}

bool NTPSnippetsLauncher::Schedule(base::TimeDelta period_wifi,
                                   base::TimeDelta period_fallback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_SnippetsLauncher_schedule(env, java_launcher_,
                                        period_wifi.InSeconds(),
                                        period_fallback.InSeconds());
}

bool NTPSnippetsLauncher::Unschedule() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_SnippetsLauncher_unschedule(env, java_launcher_);
}

bool NTPSnippetsLauncher::IsOnUnmeteredConnection() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_SnippetsLauncher_isOnUnmeteredConnection(env, java_launcher_);
}

NTPSnippetsLauncher::NTPSnippetsLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  java_launcher_.Reset(Java_SnippetsLauncher_create(env));
}

NTPSnippetsLauncher::~NTPSnippetsLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsLauncher_destroy(env, java_launcher_);
  java_launcher_.Reset();
}
