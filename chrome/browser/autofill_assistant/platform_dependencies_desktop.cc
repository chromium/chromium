// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/autofill_assistant/platform_dependencies_desktop.h"

namespace autofill_assistant {

PlatformDependenciesDesktop::PlatformDependenciesDesktop() = default;

bool PlatformDependenciesDesktop::IsCustomTab(
    const content::WebContents& web_contents) const {
  return false;
}

}  // namespace autofill_assistant
