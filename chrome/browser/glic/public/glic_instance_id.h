// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_ID_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_ID_H_

#include <string>

#include "base/types/strong_alias.h"

namespace glic {

// Instance IDs are created in the form `<index>-<64-bit-random-int>`.
// The index is an indicator of how many instances have been created by the
// profile since Chrome start. The random number is included so that instance
// IDs can be loaded from disk when restoring tabs after a browser restart.
class InstanceId : public base::StrongAlias<class InstanceIdTag, std::string> {
 public:
  using Base = base::StrongAlias<class InstanceIdTag, std::string>;
  using Base::Base;

  static InstanceId Create(uint64_t glic_instance_coordinator_id,
                           uint32_t index);
  static InstanceId CreateNullId() { return InstanceId(""); }
  // Returns true if the instance ID is valid and not null.
  bool IsValid() const { return !Base::value().empty(); }
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_ID_H_
