// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_
#define CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

class Profile;
class TabAndroid;

namespace glic {

class GlicKeyedService;

// Helper class responsible for bridging the GlicKeyedService between
// C++ and Java.
class GlicKeyedServiceAndroid : public base::SupportsUserData::Data {
 public:
  explicit GlicKeyedServiceAndroid(GlicKeyedService* service);
  ~GlicKeyedServiceAndroid() override;

  GlicKeyedServiceAndroid(const GlicKeyedServiceAndroid&) = delete;
  GlicKeyedServiceAndroid& operator=(const GlicKeyedServiceAndroid&) = delete;

  // JNI bridge to show, summon, or activate the panel, or close it if it is
  // already active.
  // `env` is the JNI environment.
  // `browser_window_ptr` is unpacked to call the cross-platform service.
  // `prevent_close` whether to prevent closing the UI if it's already open.
  // `profile` associated with this request.
  // `source` for the UI toggle.
  void ToggleUI(JNIEnv* env,
                int64_t browser_window_ptr,
                bool prevent_close,
                Profile* profile,
                int32_t source);

  bool InvokeWithAutoSubmit(JNIEnv* env,
                            TabAndroid* tab,
                            std::string text,
                            int32_t source);

  bool InvokeWithPrompt(JNIEnv* env,
                        TabAndroid* tab,
                        std::string text,
                        int32_t source);

  void Invoke(JNIEnv* env, TabAndroid* tab, int32_t source);

  void InvokeWithConversation(JNIEnv* env,
                              TabAndroid* tab,
                              std::string glic_conversation_id,
                              int32_t source);

  bool IsPanelShowingForBrowser(JNIEnv* env, int64_t browser_window_ptr);

  bool GetUserEnabledActuationOnWeb(JNIEnv* env);
  void SetUserEnabledActuationOnWeb(JNIEnv* env, bool enabled);

  bool GetExperimentalTriggeringEnabled(JNIEnv* env);
  void SetExperimentalTriggeringEnabled(JNIEnv* env, bool enabled);

  // Shares (pins) `tabs` with a Glic conversation from the tab context menu.
  // Starts a new conversation when `new_conversation` is true, otherwise shares
  // with the existing conversation identified by `instance_id`.
  void ShareTabs(JNIEnv* env,
                 std::vector<TabAndroid*> tabs,
                 std::string instance_id,
                 bool new_conversation,
                 int32_t source);

  // Unshares (unpins) `tabs` from all Glic conversations.
  void UnshareTabs(JNIEnv* env, std::vector<TabAndroid*> tabs);

  // Returns whether any of `tabs` is currently pinned to a Glic conversation.
  bool IsTabPinnedToAnyInstance(JNIEnv* env, std::vector<TabAndroid*> tabs);

  // Returns up to `limit` recently active Glic conversations as a flattened
  // [id0, title0, id1, title1, ...] vector. The Java side
  // (GlicKeyedServiceImpl) reassembles consecutive (id, title) pairs into
  // ConversationInfo objects; a flattened string vector keeps the JNI
  // signature simple.
  std::vector<std::string> GetRecentlyActiveInstances(JNIEnv* env,
                                                      int32_t limit);

  void OnGlobalShowHide();
  void OnUserEnabledActuationOnWebChanged();
  void OnExperimentalTriggeringEnabledChanged();
  void OnAllowedStateChanged();

  // Returns the GlicKeyedServiceImpl java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // Not owned.
  raw_ptr<GlicKeyedService> service_;

  // A reference to the Java counterpart of this class. See
  // GlicKeyedServiceImpl.java.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  base::CallbackListSubscription global_show_hide_subscription_;
  base::CallbackListSubscription web_actuation_pref_subscription_;
  base::CallbackListSubscription experimental_triggering_pref_subscription_;
  base::CallbackListSubscription allowed_changed_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_KEYED_SERVICE_ANDROID_H_
