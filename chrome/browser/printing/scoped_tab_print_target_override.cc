// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/scoped_tab_print_target_override.h"

#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace printing {

DEFINE_USER_DATA(ScopedTabPrintTargetOverride);

ScopedTabPrintTargetOverride::ScopedTabPrintTargetOverride(
    tabs::TabInterface& tab,
    content::WebContents& target)
    : target_(target.GetWeakPtr()),
      scoped_user_data_(tab.GetUnownedUserDataHost(), *this) {}

ScopedTabPrintTargetOverride::~ScopedTabPrintTargetOverride() = default;

}  // namespace printing
