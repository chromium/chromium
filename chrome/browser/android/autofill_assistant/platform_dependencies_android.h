// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_PLATFORM_DEPENDENCIES_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_PLATFORM_DEPENDENCIES_ANDROID_H_

#include "components/autofill_assistant/browser/platform_dependencies.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Implementation of the PlatformDependencies interface for Chrome Android.
class PlatformDependenciesAndroid : public PlatformDependencies {
 public:
  PlatformDependenciesAndroid();

  bool IsCustomTab(const content::WebContents& web_contents) const override;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_PLATFORM_DEPENDENCIES_ANDROID_H_
