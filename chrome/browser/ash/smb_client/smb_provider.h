// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_PROVIDER_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_PROVIDER_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/smb_client/smb_file_system.h"

class Profile;

namespace ash::smb_client {

class SmbProvider : public file_system_provider::ProviderInterface {
 public:
  SmbProvider();
  SmbProvider(const SmbProvider&) = delete;
  SmbProvider& operator=(const SmbProvider&) = delete;
  ~SmbProvider() override;

  // file_system_provider::ProviderInterface overrides.
  std::unique_ptr<file_system_provider::ProvidedFileSystemInterface>
  CreateProvidedFileSystem(
      Profile* profile,
      const file_system_provider::ProvidedFileSystemInfo& file_system_info,
      file_system_provider::CacheManager* cache_manager) override;
  const file_system_provider::Capabilities& GetCapabilities() const override;
  const file_system_provider::ProviderId& GetId() const override;
  const std::string& GetName() const override;
  const file_system_provider::IconSet& GetIconSet() const override;
  file_system_provider::RequestManager* GetRequestManager() override;
  bool RequestMount(
      Profile* profile,
      ash::file_system_provider::RequestMountCallback callback) override;

 private:
  file_system_provider::ProviderId provider_id_;
  file_system_provider::Capabilities capabilities_;
  std::string name_;
  file_system_provider::IconSet icon_set_;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_PROVIDER_H_
