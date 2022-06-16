// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_

#include <ostream>
#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"

namespace guest_os {

// A unique identifier for our guests.
struct GuestId {
  GuestId(std::string vm_name, std::string container_name) noexcept;
  explicit GuestId(const base::Value&) noexcept;

  base::flat_map<std::string, std::string> ToMap() const;
  base::Value::Dict ToDictValue() const;

  std::string vm_name;
  std::string container_name;
};

bool operator<(const GuestId& lhs, const GuestId& rhs) noexcept;
bool operator==(const GuestId& lhs, const GuestId& rhs) noexcept;
inline bool operator!=(const GuestId& lhs, const GuestId& rhs) noexcept {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const GuestId& container_id);

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_
