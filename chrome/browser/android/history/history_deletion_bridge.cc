// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/history_deletion_bridge.h"

#include <string>
#include <vector>

#include "chrome/android/chrome_jni_headers/HistoryDeletionBridge_jni.h"
#include "chrome/browser/android/history/history_deletion_info.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

static jlong JNI_HistoryDeletionBridge_Init(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj) {
  return reinterpret_cast<intptr_t>(new HistoryDeletionBridge(jobj));
}

// static
history::DeletionInfo HistoryDeletionBridge::SanitizeDeletionInfo(
    const history::DeletionInfo& deletion_info) {
  std::vector<history::URLRow> sanitized_rows;
  for (auto row : deletion_info.deleted_rows()) {
    if (!row.url().is_empty() && row.url().is_valid())
      sanitized_rows.push_back(row);
  }
  return history::DeletionInfo::ForUrls(sanitized_rows, {});
}

HistoryDeletionBridge::HistoryDeletionBridge(const JavaRef<jobject>& jobj)
    : jobj_(ScopedJavaGlobalRef<jobject>(jobj)),
      profile_(ProfileManager::GetLastUsedProfile()->GetOriginalProfile()) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service)
    history_service->AddObserver(this);
}

HistoryDeletionBridge::~HistoryDeletionBridge() {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service)
    history_service->RemoveObserver(this);
}

void HistoryDeletionBridge::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = base::android::AttachCurrentThread();
  history::DeletionInfo sanitized_info = SanitizeDeletionInfo(deletion_info);
  Java_HistoryDeletionBridge_onURLsDeleted(
      env, jobj_, CreateHistoryDeletionInfo(env, &sanitized_info));
}
