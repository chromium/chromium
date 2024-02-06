// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_URL_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_URL_H_

#include <string>

#include "url/third_party/mozilla/url_parse.h"

namespace ash::smb_client {

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

  // Allow copying.
  SmbUrl(const SmbUrl& url);
  SmbUrl& operator=(const SmbUrl& url);

  // Returns the host of the URL which can be resolved or unresolved.
  std::string GetHost() const;

  // Returns the share component of the URL. This does not have to be
  // set for IsValid() to be true (but some clients may require it).
  std::string GetShare() const;

  // Returns the full, canonicalized URL |url_| in the form
  // smb://server/share/path (where share and path are optional). Will
  // not have trailing slashes.
  const std::string& ToString() const;

  // Replaces the host to |new_host| and returns the full URL. Does not
  // change the original URL.
  SmbUrl ReplaceHost(const std::string& new_host) const;

  // Returns true if the passed URL is valid and was properly parsed. This
  // should be called after the constructor. Callers should verify this
  // before trying to use ToString(), GetHost(), GetShare() etc.
  bool IsValid() const;

  // Returns |url_| in the format \\server\share.
  std::string GetWindowsUNCString() const;

 private:
  // Canonicalize |url| and saves the output as url_ and host_ if successful.
  void CanonicalizeSmbUrl(const std::string& url);

  // Parse |url| into a Windows UNC |windows_unc_|.
  void CreateWindowsUnc(const std::string& url);

  // Resets url_ and parsed_. Makes the SmbUrl invalid.
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
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_URL_H_
