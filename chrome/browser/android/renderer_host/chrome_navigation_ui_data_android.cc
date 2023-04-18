// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/ChromeNavigationUIData_jni.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/browser/navigation_ui_data.h"

static jlong JNI_ChromeNavigationUIData_CreateUnownedNativeCopy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong bookmark_id) {
  const bookmarks::BookmarkNode* node = nullptr;
  if (bookmarks::BookmarkModel* model =
          BookmarkModelFactory::GetForBrowserContext(
              ProfileManager::GetLastUsedProfile())) {
    node = bookmarks::GetBookmarkNodeByID(model, bookmark_id);
    // `bookmark_id` may be -1. This indicates that no bookmark should be
    // attached to this navigation.
    DCHECK(bookmark_id != -1 || node == nullptr);
  }

  ChromeNavigationUIData* ui_data = new ChromeNavigationUIData();
  if (node) {
    ui_data->set_bookmark_id(node->uuid());
  }
  return reinterpret_cast<intptr_t>(
      static_cast<content::NavigationUIData*>(ui_data));
}
