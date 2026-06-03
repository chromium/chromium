// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

class BrowserWindowInterface;
class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

class ActiveTaskContextProvider;
class ContextualTasksPanelController;
class ContextualTasksUiService;
class EntryPointEligibilityManager;

// Native counterpart of ContextualTasksBridge.java.
// Owned by the Java ContextualTasksBridge.
class ContextualTasksBridge {
 public:
  DECLARE_USER_DATA(ContextualTasksBridge);

  ContextualTasksBridge(JNIEnv* env,
                        const jni_zero::JavaRef<jobject>& obj,
                        BrowserWindowInterface* window,
                        Profile* profile);
  ~ContextualTasksBridge();

  // Disallow copy/assign.
  ContextualTasksBridge(const ContextualTasksBridge&) = delete;
  ContextualTasksBridge& operator=(const ContextualTasksBridge&) = delete;

  // Returns the ContextualTasksBridge for the given |window|, if one exists.
  static ContextualTasksBridge* From(BrowserWindowInterface* window);

  void Destroy(JNIEnv* env);

  // Called from Java via JNI to undo the closure of the sheet.
  void UndoClose(JNIEnv* env);

  // Called from Java via JNI to start the Android system voice recognition.
  void StartPlatformVoiceRecognition();

  // Called from Java via JNI to send voice search results to WebUI.
  void OnVoiceTranscribed(JNIEnv* env, const std::string& query);

  // Notification methods to call into Java.
  void NotifyWebUIReady(const base::Uuid& task_id,
                        content::WebContents* web_contents);
  void NotifyWebUIDestroyed(const std::optional<base::Uuid>& task_id);
  void NotifyTaskChanged(const std::optional<base::Uuid>& old_task_id,
                         const std::optional<base::Uuid>& new_task_id);
  void NotifyShowUndoSnackbar();
  void NotifyOpenFeedbackUi(const GURL& page_url);

 private:
  // Factory to emulate browser_window_features.cc's functionality for browser-
  // scoped feature on Android. Once there is an implementation of
  // browser_window_features for Android, move the instantiation of features
  // owned by this factory to that.
  static ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
  GetUserDataFactory();

  // Non-owning reference to the profile. This is passed from the Java code so
  // the object should not be owned by this bridge.
  raw_ptr<Profile> profile_ = nullptr;

  // Cached reference to the UI service for this profile. Used to route
  // events (like voice transcription) back to the service.
  raw_ptr<ContextualTasksUiService> contextual_tasks_ui_service_ = nullptr;

  // The provider that tracks the task associated with the active tab.
  std::unique_ptr<ActiveTaskContextProvider> active_task_context_provider_;

  // The manager that determines whether the entry points are eligible to be
  // shown.
  std::unique_ptr<contextual_tasks::EntryPointEligibilityManager>
      entry_point_eligibility_manager_;

  // The interface to interact with the bottom sheet ContextualTasks panel.
  std::unique_ptr<ContextualTasksPanelController> controller_;

  jni_zero::ScopedJavaGlobalRef<jobject> java_obj_;

  // Handles the registration and discovery of this bridge via the window's
  // UnownedUserDataHost.
  ui::ScopedUnownedUserData<ContextualTasksBridge> scoped_unowned_user_data_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_
