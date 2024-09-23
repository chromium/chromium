// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history/history_deletion_bridge.h"

#include <string>
#include <vector>

#include "chrome/browser/android/history/history_deletion_info.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/HistoryDeletionBridge_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;

static jlong JNI_HistoryDeletionBridge_Init(JNIEnv* env,
                                            const JavaParamRef<jobject>& jobj,
                                            Profile* profile) {
  return reinterpret_cast<intptr_t>(new HistoryDeletionBridge(jobj, profile));
}

// static
history::DeletionInfo HistoryDeletionBridge::SanitizeDeletionInfo(
    const history::DeletionInfo& deletion_info) {
  std::vector<history::URLRow> sanitized_rows;
  for (auto row : deletion_info.deleted_rows()) {
    if (!row.url().is_empty() && row.url().is_valid())
      sanitized_rows.push_back(row);
  }
  return history::DeletionInfo(
      deletion_info.time_range(), deletion_info.is_from_expiration(),
      /*deleted_rows=*/sanitized_rows, deletion_info.favicon_urls(),
      deletion_info.restrict_urls());
}

HistoryDeletionBridge::HistoryDeletionBridge(const JavaRef<jobject>& jobj,
                                             Profile* profile)
    : jobj_(ScopedJavaGlobalRef<jobject>(jobj)) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (history_service)
    scoped_history_service_observer_.Observe(history_service);
}

HistoryDeletionBridge::~HistoryDeletionBridge() = default;

void HistoryDeletionBridge::Destroy(JNIEnv* env) {
  delete this;
}

void HistoryDeletionBridge::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = jni_zero::AttachCurrentThread();
  history::DeletionInfo sanitized_info = SanitizeDeletionInfo(deletion_info);
  Java_HistoryDeletionBridge_onURLsDeleted(
      env, jobj_, CreateHistoryDeletionInfo(env, &sanitized_info));
}

void HistoryDeletionBridge::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  scoped_history_service_observer_.Reset();
}
