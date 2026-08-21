// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/isolated_mode/settings.h"

#include "chrome/common/channel_info.h"
#include "components/enterprise/isolated_mode/settings.h"
#include "components/prefs/pref_service.h"

namespace enterprise_isolated_mode {

bool IsolatedModeReplacesIncognito(const PrefService& pref_service) {
  return enterprise_isolated_mode::IsolatedModeReplacesIncognito(
      pref_service, chrome::GetChannel());
}

}  // namespace enterprise_isolated_mode
