// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_provider.h"

#include <utility>

#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/smb_client/smb_file_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_share_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash::smb_client {

SmbProvider::SmbProvider()
    : provider_id_(file_system_provider::ProviderId::CreateFromNativeId("smb")),
      capabilities_{.configurable = false,
                    .watchable = false,
                    .multiple_mounts = true,
                    .source = extensions::SOURCE_NETWORK},
      name_(l10n_util::GetStringUTF8(IDS_SMB_SHARES_ADD_SERVICE_MENU_OPTION)) {}

SmbProvider::~SmbProvider() = default;

std::unique_ptr<file_system_provider::ProvidedFileSystemInterface>
SmbProvider::CreateProvidedFileSystem(
    Profile* profile,
    const file_system_provider::ProvidedFileSystemInfo& file_system_info,
    file_system_provider::CacheManager* cache_manager) {
  DCHECK(profile);
  return std::make_unique<SmbFileSystem>(file_system_info);
}

const file_system_provider::Capabilities& SmbProvider::GetCapabilities() const {
  return capabilities_;
}

const file_system_provider::ProviderId& SmbProvider::GetId() const {
  return provider_id_;
}

const std::string& SmbProvider::GetName() const {
  return name_;
}

const file_system_provider::IconSet& SmbProvider::GetIconSet() const {
  // Returns an empty IconSet.
  return icon_set_;
}

file_system_provider::RequestManager* SmbProvider::GetRequestManager() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool SmbProvider::RequestMount(
    Profile* profile,
    ash::file_system_provider::RequestMountCallback callback) {
  // Mount requests for SMB are handled by the SMB dialog. The callback
  // isn't expected to be used.
  std::move(callback).Run(base::File::Error::FILE_OK);
  smb_dialog::SmbShareDialog::Show();
  return true;
}

}  // namespace ash::smb_client
