// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_DEVICE_DESCRIPTION_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_DEVICE_DESCRIPTION_H_

#include <string>

#include "net/base/host_port_pair.h"

namespace local_discovery {
struct ServiceDescription;
}

namespace cloud_print {

struct DeviceDescription {
  DeviceDescription();
  explicit DeviceDescription(
      const local_discovery::ServiceDescription& service_description);
  DeviceDescription(const DeviceDescription& other);
  ~DeviceDescription();

  bool IsValid() const;

  // Display attributes
  std::string name;
  std::string description;

  // Functional attributes
  std::string id;
  std::string type;
  int version;

  // Attributes related to local HTTP
  net::HostPortPair address;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_DEVICE_DESCRIPTION_H_
