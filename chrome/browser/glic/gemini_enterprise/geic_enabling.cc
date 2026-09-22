// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "url/gurl.h"

namespace geic {

bool IsGeicEnabled() {
  if (!base::FeatureList::IsEnabled(features::kGeic)) {
    return false;
  }

  return features::kGeicEnabledParam.Get();
}

GURL GetGeicGuestUrl() {
  if (!IsGeicEnabled()) {
    return GURL();
  }

  // 1. Check command-line override switch.
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kGeicGuestURLSwitch)) {
    return GURL(command_line->GetSwitchValueASCII(kGeicGuestURLSwitch));
  }

  // 2. Finch parameter: features::kGeicGuestURL.
  return GURL(features::kGeicGuestURL.Get());
}

}  // namespace geic
