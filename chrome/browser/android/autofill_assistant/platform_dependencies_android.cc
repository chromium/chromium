// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/android/autofill_assistant/platform_dependencies_android.h"

#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

PlatformDependenciesAndroid::PlatformDependenciesAndroid() = default;

bool PlatformDependenciesAndroid::IsCustomTab(
    const content::WebContents& web_contents) const {
  auto* tab_android = TabAndroid::FromWebContents(&web_contents);
  if (!tab_android) {
    return false;
  }

  return tab_android->IsCustomTab();
}

}  // namespace autofill_assistant
