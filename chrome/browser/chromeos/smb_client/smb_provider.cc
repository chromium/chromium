// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_provider.h"

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/smb_client/smb_file_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace chromeos {
namespace smb_client {

SmbProvider::SmbProvider(UnmountCallback unmount_callback)
    : provider_id_(ProviderId::CreateFromNativeId("smb")),
      capabilities_(false /* configurable */,
                    false /* watchable */,
                    true /* multiple_mounts */,
                    extensions::SOURCE_NETWORK),
      name_(l10n_util::GetStringUTF8(IDS_SMB_SHARES_ADD_SERVICE_MENU_OPTION)),
      unmount_callback_(std::move(unmount_callback)) {
  icon_set_.SetIcon(IconSet::IconSize::SIZE_16x16,
                    GURL("chrome://theme/IDR_SMB_ICON"));
  icon_set_.SetIcon(IconSet::IconSize::SIZE_32x32,
                    GURL("chrome://theme/IDR_SMB_ICON@2x"));
}

SmbProvider::~SmbProvider() = default;

std::unique_ptr<ProvidedFileSystemInterface>
SmbProvider::CreateProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info) {
  DCHECK(profile);
  return std::make_unique<SmbFileSystem>(file_system_info, unmount_callback_);
}

const Capabilities& SmbProvider::GetCapabilities() const {
  return capabilities_;
}

const ProviderId& SmbProvider::GetId() const {
  return provider_id_;
}

const std::string& SmbProvider::GetName() const {
  return name_;
}

const IconSet& SmbProvider::GetIconSet() const {
  return icon_set_;
}

bool SmbProvider::RequestMount(Profile* profile) {
  auto* settings_manager = chrome::SettingsWindowManager::GetInstance();
  settings_manager->ShowChromePageForProfile(
      profile, chrome::GetSettingsUrl(chrome::kSmbSharesPageAddDialog));

  return true;
}

}  // namespace smb_client
}  // namespace chromeos
