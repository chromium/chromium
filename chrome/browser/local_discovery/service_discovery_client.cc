// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/local_discovery/service_discovery_client.h"

namespace local_discovery {

ServiceDescription::ServiceDescription() {
}

ServiceDescription::ServiceDescription(const ServiceDescription& other) =
    default;

ServiceDescription::~ServiceDescription() {
}

std::string ServiceDescription::instance_name() const {
  // TODO(noamsml): Once we have escaping working, get this to
  // parse escaped domains.
  size_t first_period = service_name.find_first_of('.');
  return service_name.substr(0, first_period);
}

std::string ServiceDescription::service_type() const {
  // TODO(noamsml): Once we have escaping working, get this to
  // parse escaped domains.
  size_t first_period = service_name.find_first_of('.');
  if (first_period == std::string::npos)
    return "";
  return service_name.substr(first_period+1);
}

}  // namespace local_discovery
