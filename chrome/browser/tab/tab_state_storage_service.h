// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_STATE_STORAGE_SERVICE_H_
#define CHROME_BROWSER_TAB_TAB_STATE_STORAGE_SERVICE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/tab/tab_state_storage_backend.h"
#include "components/keyed_service/core/keyed_service.h"

namespace tabs {

class TabStateStorageService : public KeyedService {
 public:
  explicit TabStateStorageService(
      std::unique_ptr<TabStateStorageBackend> tab_backend);
  ~TabStateStorageService() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void SaveTab(JNIEnv* env,
               int id,
               int parent_collection_id,
               std::string position,
               int parent_tab_id,
               int root_id,
               long timestamp_millis,
               const jni_zero::JavaParamRef<jobject>& web_contents_state_buffer,
               std::string opener_app_id,
               int theme_color,
               int launch_type_at_creation,
               int user_agent,
               long last_navigation_committed_timestamp_millis,
               const jni_zero::JavaParamRef<jobject>& j_tab_group_id,
               bool tab_has_sensitive_content,
               bool is_pinned);

  void LoadAllTabs(JNIEnv* env,
                   const jni_zero::JavaParamRef<jobject>& j_callback);

 private:
  std::unique_ptr<TabStateStorageBackend> tab_backend_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_STATE_STORAGE_SERVICE_H_
