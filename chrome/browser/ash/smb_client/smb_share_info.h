// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SHARE_INFO_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SHARE_INFO_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/smb_client/smb_url.h"

namespace ash::smb_client {

// Common parameters for SMB shares.
// Note: Password is explicitly excluded here. Due to being sensitive
// information, it should be considered separate to other parameters.
class SmbShareInfo {
 public:
  SmbShareInfo(const SmbUrl& share_url,
               const std::string& display_name,
               const std::string& username,
               const std::string& workgroup,
               bool use_kerberos,
               const std::vector<uint8_t>& password_salt = {});
  ~SmbShareInfo();

  // Allow copies.
  SmbShareInfo(const SmbShareInfo&);
  SmbShareInfo& operator=(const SmbShareInfo&);

  // Disallow creating an empty instance.
  SmbShareInfo() = delete;

  const SmbUrl& share_url() const { return share_url_; }
  const std::string& display_name() const { return display_name_; }
  const std::string& username() const { return username_; }
  const std::string& workgroup() const { return workgroup_; }
  bool use_kerberos() const { return use_kerberos_; }
  const std::vector<uint8_t>& password_salt() const { return password_salt_; }

 private:
  SmbUrl share_url_;
  std::string display_name_;
  std::string username_;
  std::string workgroup_;
  bool use_kerberos_ = false;
  std::vector<uint8_t> password_salt_;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SHARE_INFO_H_
