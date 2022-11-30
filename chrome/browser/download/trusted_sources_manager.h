// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_TRUSTED_SOURCES_MANAGER_H_
#define CHROME_BROWSER_DOWNLOAD_TRUSTED_SOURCES_MANAGER_H_

#include <memory>

#include "net/base/scheme_host_port_matcher.h"

class GURL;

// Identifies if a URL is from a trusted source.
class TrustedSourcesManager {
 public:
  TrustedSourcesManager(const TrustedSourcesManager&) = delete;
  TrustedSourcesManager& operator=(const TrustedSourcesManager&) = delete;

  virtual ~TrustedSourcesManager();

  // Creates a platform-dependent instance of TrustedSourcesManager.
  //
  // A trusted sources manager has a list of sources that can be trusted with
  // downloads, extracted from the kTrustedDownloadSources command line switch.
  // An example usage is to specify that files downloaded from trusted sites
  // don't need to be scanned by SafeBrowsing when the
  // SafeBrowsingForTrustedSourcesEnabled policy is set to false.
  //
  // On creation the list of trusted sources is NULL.
  //
  // If the platform is Windows, the kTrustedDownloadSources value is ignored,
  // the security zone mapping is used instead to determine whether the source
  // is trusted or not.
  //
  static std::unique_ptr<TrustedSourcesManager> Create();

  // Returns true if the source of this URL is part of the trusted sources.
  virtual bool IsFromTrustedSource(const GURL& url) const;

 protected:
  // Must use Create.
  TrustedSourcesManager();

 private:
  net::SchemeHostPortMatcher matcher_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_TRUSTED_SOURCES_MANAGER_H_
