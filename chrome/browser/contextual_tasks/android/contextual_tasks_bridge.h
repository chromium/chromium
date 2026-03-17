// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

class BrowserWindowInterface;
class Profile;

namespace contextual_tasks {

class ActiveTaskContextProvider;
class ContextualTasksPanelController;

// Native counterpart of ContextualTasksBridge.java.
// Owned by the Java ContextualTasksBridge.
class ContextualTasksBridge {
 public:
  ContextualTasksBridge(JNIEnv* env,
                        const jni_zero::JavaRef<jobject>& obj,
                        BrowserWindowInterface* window,
                        Profile* profile);
  ~ContextualTasksBridge();

  // Disallow copy/assign.
  ContextualTasksBridge(const ContextualTasksBridge&) = delete;
  ContextualTasksBridge& operator=(const ContextualTasksBridge&) = delete;

  void Destroy(JNIEnv* env);

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

  // The provider that tracks the task associated with the active tab.
  std::unique_ptr<ActiveTaskContextProvider> active_task_context_provider_;

  // The interface to interact with the bottom sheet ContextualTasks panel.
  std::unique_ptr<ContextualTasksPanelController> controller_;

  jni_zero::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_BRIDGE_H_
