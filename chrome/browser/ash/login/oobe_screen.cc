// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_screen.h"

#include <ostream>

namespace ash {

OobeScreenId::OobeScreenId(const std::string& id) : name(id) {}

OobeScreenId::OobeScreenId(const StaticOobeScreenId& id)
    : name(id.name) {
  if (id.external_api_prefix)
    external_api_prefix = id.external_api_prefix;
  // TODO(https://crbug.com/1312879): Uncomment when the bug is fixed.
  // DCHECK(!external_api_prefix.empty());
}

bool OobeScreenId::operator==(const OobeScreenId& rhs) const {
  return name == rhs.name;
}

bool OobeScreenId::operator!=(const OobeScreenId& rhs) const {
  return name != rhs.name;
}

bool OobeScreenId::operator<(const OobeScreenId& rhs) const {
  return name < rhs.name;
}

std::ostream& operator<<(std::ostream& stream, const OobeScreenId& id) {
  return stream << id.name;
}

OobeScreenId StaticOobeScreenId::AsId() const {
  return OobeScreenId(name);
}

ScreenSummary::ScreenSummary() = default;
ScreenSummary::~ScreenSummary() = default;
ScreenSummary::ScreenSummary(const ScreenSummary& summary) = default;

}  // namespace ash
