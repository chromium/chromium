// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_URL_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_URL_H_

#include <string>

#include "base/macros.h"
#include "url/third_party/mozilla/url_parse.h"

namespace chromeos {
namespace smb_client {

// Represents an SMB URL.
// This class stores a URL using url::Component and can contain either a
// resolved or unresolved host. The host can be replaced when the address is
// resolved by using ReplaceHost(). The passed URL must start with either
// "smb://" or "\\" when constructed.
class SmbUrl {
 public:
  explicit SmbUrl(const std::string& url);
  ~SmbUrl();
  SmbUrl(SmbUrl&& smb_url);

  // Returns the host of the URL which can be resolved or unresolved.
  std::string GetHost() const;

  // Returns the share component of the URL.
  std::string GetShare() const;

  // Returns the full URL.
  const std::string& ToString() const;

  // Replaces the host to |new_host| and returns the full URL. Does not
  // change the original URL.
  std::string ReplaceHost(const std::string& new_host) const;

  // Returns true if the passed URL is valid and was properly parsed. This
  // should be called after the constructor.
  bool IsValid() const;

  // Returns |url_| in the format \\server\share.
  std::string GetWindowsUNCString() const;

 private:
  // Canonicalize |url| and saves the output as url_ and host_ if successful.
  void CanonicalizeSmbUrl(const std::string& url);

  // Parse |url| into a Windows UNC |windows_unc_|.
  void CreateWindowsUnc(const std::string& url);

  // Resets url_ and parsed_.
  void Reset();

  // String form of the canonical url.
  std::string url_;

  // String form of the Windows Universal Naming Convention of the url.
  // MS-DTYP section 2.2.57
  std::string windows_unc_;

  // Holds the identified host of the URL. This does not store the host itself.
  url::Component host_;

  // Share name component of the URL.
  std::string share_;

  DISALLOW_COPY_AND_ASSIGN(SmbUrl);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_URL_H_
