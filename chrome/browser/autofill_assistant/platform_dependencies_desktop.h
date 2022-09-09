// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PLATFORM_DEPENDENCIES_DESKTOP_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PLATFORM_DEPENDENCIES_DESKTOP_H_

#include "components/autofill_assistant/browser/platform_dependencies.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Implementation of the PlatformDependencies interface for desktop.
class PlatformDependenciesDesktop : public PlatformDependencies {
 public:
  PlatformDependenciesDesktop();

  bool IsCustomTab(const content::WebContents& web_contents) const override;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PLATFORM_DEPENDENCIES_DESKTOP_H_
