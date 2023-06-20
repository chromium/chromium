// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace history {
class HistoryService;
}  // namespace history

using base::android::JavaParamRef;

// The bridge for fetching information and executing commands for the Android
// Quick Delete UI.
class QuickDeleteBridge {
 public:
  explicit QuickDeleteBridge(Profile* profile);

  QuickDeleteBridge(const QuickDeleteBridge&) = delete;
  QuickDeleteBridge& operator=(const QuickDeleteBridge&) = delete;
  ~QuickDeleteBridge();

  void Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj);

 private:
  raw_ptr<Profile> profile_;

  raw_ptr<history::HistoryService> history_service_;

  base::WeakPtrFactory<QuickDeleteBridge> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_QUICK_DELETE_QUICK_DELETE_BRIDGE_H_
