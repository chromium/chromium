// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

namespace send_tab_to_self {

class SendTabToSelfModel;

// The delegate to observe for SendTabToSelf model changes and forward them to
// observers of this class. The class is owned by the SendTabToSelf Java
// counterpart and lives for the duration of the life of that class.
class SendTabToSelfModelObserverBridge : public SendTabToSelfModelObserver {
 public:
  SendTabToSelfModelObserverBridge(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jobject>& j_profile);

  void Destroy(JNIEnv*);

  // SendTabToSelfModelObserver implementation.
  void SendTabToSelfModelLoaded() override;
  void EntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override;
  void EntriesRemovedRemotely(const std::vector<std::string>& guids) override;

 protected:
  ~SendTabToSelfModelObserverBridge() override;

 private:
  JavaObjectWeakGlobalRef weak_java_ref_;
  // Set during the constructor and owned by the SendTabToSelfSyncServiceFactory
  // is based off the KeyedServiceFactory which lives for the length of the
  // profile. SendTabToSelf is not supported for the Incognito profile.
  SendTabToSelfModel* send_tab_to_self_model_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfModelObserverBridge);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_BRIDGE_H_
