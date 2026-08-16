// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_enabling.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"

namespace geic {

bool IsGeicEnabled(Profile* profile) {
  if (!profile || !profile->IsRegularProfile()) {
    return false;
  }
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kGeicEnabled)) {
    return false;
  }
  const auto channel = chrome::GetChannel();
  const bool is_developer_build = !version_info::IsOfficialBuild() ||
                                  channel == version_info::Channel::UNKNOWN ||
                                  channel == version_info::Channel::CANARY;
  // TODO(crbug.com/545155625): Remove --disable-web-security requirement before
  // teamfood. Currently required so users see a security warning banner if led
  // to enabling this via social engineering.
  if (!is_developer_build &&
      !command_line->HasSwitch(::switches::kDisableWebSecurity)) {
    return false;
  }
  return !base::FeatureList::IsEnabled(features::kGlic);
}

}  // namespace geic
