// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/core/crow/crow_configuration.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/singleton.h"
#include "chrome/browser/share/proto/crow_configuration.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.h"

namespace crow {

using base::AutoLock;

struct CrowConfigurationSingletonTrait
    : public base::DefaultSingletonTraits<CrowConfiguration> {
  static CrowConfiguration* New() {
    CrowConfiguration* instance = new CrowConfiguration();
    return instance;
  }
};

// static
CrowConfiguration* CrowConfiguration::GetInstance() {
  return base::Singleton<CrowConfiguration,
                         CrowConfigurationSingletonTrait>::get();
}

CrowConfiguration::CrowConfiguration() = default;

CrowConfiguration::~CrowConfiguration() {
  AutoLock lock(lock_);  // DCHECK fail if the lock is held.
}

void CrowConfiguration::PopulateFromBinaryPb(const std::string& binary_pb) {
  AutoLock lock(lock_);

  // TODO(crbug.com/1324766): Report success/failure via metrics.
  if (binary_pb.empty())
    return;

  auto config = std::make_unique<crow::mojom::CrowConfiguration>();
  if (!config->ParseFromString(binary_pb))
    return;

  domains_.clear();
  for (const crow::mojom::Publisher& publisher : config->publisher()) {
    for (const std::string& host : publisher.host()) {
      domains_.insert(std::make_pair(host, publisher.publication_id()));
    }
  }

  denied_hosts_.clear();
  for (const std::string& host : config->denied_hosts()) {
    denied_hosts_.insert(host);
  }
}

std::string CrowConfiguration::GetPublicationIDFromAllowlist(
    const std::string& host) const {
  AutoLock lock(lock_);

  return domains_.count(host) > 0 ? domains_.at(host) : "";
}

bool CrowConfiguration::DenylistContainsHost(const std::string& host) const {
  AutoLock lock(lock_);

  return denied_hosts_.count(host) > 0;
}

}  // namespace crow
