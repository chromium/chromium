// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/tls_deprecation_config.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "services/network/public/proto/tls_deprecation_config.pb.h"
#include "url/gurl.h"

namespace {

class TLSDeprecationConfigSingleton {
 public:
  void SetProto(
      std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto) {
    // Check that the version id is set (otherwise tests can fail in confusing
    // ways).
    DCHECK(proto->has_version_id());
    proto_ = std::move(proto);
  }

  chrome_browser_ssl::LegacyTLSExperimentConfig* GetProto() const {
    return proto_.get();
  }

  void Reset() { proto_.reset(); }

  static TLSDeprecationConfigSingleton& GetInstance() {
    static base::NoDestructor<TLSDeprecationConfigSingleton> instance;
    return *instance;
  }

 private:
  std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto_;
};

}  // namespace

void SetRemoteTLSDeprecationConfig(const std::string& binary_config) {
  auto proto =
      std::make_unique<chrome_browser_ssl::LegacyTLSExperimentConfig>();
  if (binary_config.empty() || !proto->ParseFromString(binary_config)) {
    DVLOG(1) << "Failed parsing legacy TLS config proto";
    return;
  }
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

void ResetTLSDeprecationConfigForTesting() {
  TLSDeprecationConfigSingleton::GetInstance().Reset();
}
