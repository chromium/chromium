// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_LOAD_TERMINATION_LISTENER_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_LOAD_TERMINATION_LISTENER_IMPL_H_

#include "base/android/application_status_listener.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/load_termination_listener.h"

namespace offline_pages {

// Implementation of LoadTerminationListener for Android.
class LoadTerminationListenerImpl : public LoadTerminationListener {
 public:
  LoadTerminationListenerImpl();
  ~LoadTerminationListenerImpl() override;

  // Callback
  void OnApplicationStateChange(
      base::android::ApplicationState application_state);

 private:
  // An instance of Android AppListener.
  std::unique_ptr<base::android::ApplicationStatusListener> app_listener_;

  base::WeakPtrFactory<LoadTerminationListenerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoadTerminationListenerImpl);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_LOAD_TERMINATION_LISTENER_IMPL_H_
