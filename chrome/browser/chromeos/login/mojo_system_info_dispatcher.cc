// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mojo_system_info_dispatcher.h"

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/channel.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

MojoSystemInfoDispatcher::MojoSystemInfoDispatcher() = default;

MojoSystemInfoDispatcher::~MojoSystemInfoDispatcher() = default;

void MojoSystemInfoDispatcher::StartRequest() {
#if defined(OFFICIAL_BUILD)
  version_info_updater_.StartUpdate(true /*is_official_build*/);
#else
  version_info_updater_.StartUpdate(false /*is_official_build*/);
#endif
}

void MojoSystemInfoDispatcher::OnOSVersionLabelTextUpdated(
    const std::string& os_version_label_text) {
  os_version_label_text_ = os_version_label_text;
  OnSystemInfoUpdated();
}

void MojoSystemInfoDispatcher::OnEnterpriseInfoUpdated(
    const std::string& enterprise_info,
    const std::string& asset_id) {
  if (asset_id.empty())
    return;
  enterprise_info_ = l10n_util::GetStringFUTF8(IDS_OOBE_ASSET_ID_LABEL,
                                               base::UTF8ToUTF16(asset_id));
  OnSystemInfoUpdated();
}

void MojoSystemInfoDispatcher::OnDeviceInfoUpdated(
    const std::string& bluetooth_name) {
  bluetooth_name_ = bluetooth_name;
  OnSystemInfoUpdated();
}

void MojoSystemInfoDispatcher::OnAdbSideloadStatusUpdated(bool enabled) {
  adb_sideloading_enabled_ = enabled;
  OnSystemInfoUpdated();
}

void MojoSystemInfoDispatcher::OnSystemInfoUpdated() {
  const base::Optional<bool> policy_show =
      version_info_updater_.IsSystemInfoEnforced();
  bool enforced = policy_show.has_value();
  bool show = false;
  if (enforced) {
    show = policy_show.value();
  } else {
    version_info::Channel channel = chrome::GetChannel();
    show = channel != version_info::Channel::STABLE &&
           channel != version_info::Channel::BETA;
  }
  ash::LoginScreen::Get()->GetModel()->SetSystemInfo(
      show, enforced, os_version_label_text_, enterprise_info_, bluetooth_name_,
      adb_sideloading_enabled_);
}

}  // namespace chromeos
