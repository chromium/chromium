// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/tls_deprecation_config.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ssl/tls_deprecation_config.pb.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace {

class TLSDeprecationConfigSingleton {
 public:
  void SetProto(
      std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto) {
    proto_ = std::move(proto);
  }

  chrome_browser_ssl::LegacyTLSExperimentConfig* GetProto() const {
    return proto_.get();
  }

  static TLSDeprecationConfigSingleton& GetInstance() {
    static base::NoDestructor<TLSDeprecationConfigSingleton> instance;
    return *instance;
  }

 private:
  std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto_;
};

}  // namespace

void SetRemoteTLSDeprecationConfigProto(
    std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto) {
  TLSDeprecationConfigSingleton::GetInstance().SetProto(std::move(proto));
}

bool ShouldSuppressLegacyTLSWarning(const GURL& url) {
  if (!url.has_host() || !url.SchemeIsCryptographic())
    return false;

  auto* proto = TLSDeprecationConfigSingleton::GetInstance().GetProto();
  // If the config is not yet loaded, we err on the side of not showing warnings
  // for any sites.
  if (!proto)
    return true;

  // Convert bytes from crypto::SHA256 so we can compare to the proto contents.
  std::string host_hash_bytes = crypto::SHA256HashString(url.host_piece());
  std::string host_hash = base::ToLowerASCII(
      base::HexEncode(host_hash_bytes.c_str(), host_hash_bytes.size()));
  const auto& control_site_hashes = proto->control_site_hashes();

  // Perform binary search on the sorted list of control site hashes to check
  // if the input URL's hostname is included.
  auto lower = std::lower_bound(control_site_hashes.begin(),
                                control_site_hashes.end(), host_hash);

  return lower != control_site_hashes.end() && *lower == host_hash;
}
