// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate_android.h"

#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksUiServiceDelegateAndroid::
    ContextualTasksUiServiceDelegateAndroid(Profile* profile)
    : ContextualTasksUiServiceDelegate(profile) {}

ContextualTasksUiServiceDelegateAndroid::
    ~ContextualTasksUiServiceDelegateAndroid() = default;

void ContextualTasksUiServiceDelegateAndroid::OpenHelpUi(
    BrowserWindowInterface* browser,
    const GURL& page_url) {
  // TODO(crbug.com/493911541): Implement OpenHelpUi in Android platform.
  return;
}

}  // namespace contextual_tasks
