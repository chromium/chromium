// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_screen.h"

#include <ostream>

namespace chromeos {

OobeScreenId::OobeScreenId(const std::string& name) : name(name) {}

OobeScreenId::OobeScreenId(const StaticOobeScreenId& id) : name(id.name) {}

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

// OobeScreenId instances should always be attached to their associated handler;
// the list below contains only OobeScreenId instances that do not have a
// handler.
//
// Ideally this list should contain only special or helper screens, e.g., those
// without a JS counterpart.
//
// TODO(crbug.com/958905): Reduce this list to only special or helper screens

// static
constexpr StaticOobeScreenId
    OobeScreen::SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED;
constexpr StaticOobeScreenId OobeScreen::SCREEN_CONFIRM_PASSWORD;
constexpr StaticOobeScreenId OobeScreen::SCREEN_UNKNOWN;

}  // namespace chromeos
