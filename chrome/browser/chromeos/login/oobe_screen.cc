// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oobe_screen.h"

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
constexpr StaticOobeScreenId OobeScreen::SCREEN_ACCOUNT_PICKER;
constexpr StaticOobeScreenId OobeScreen::SCREEN_TPM_ERROR;
constexpr StaticOobeScreenId OobeScreen::SCREEN_PASSWORD_CHANGED;
constexpr StaticOobeScreenId
    OobeScreen::SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED;
constexpr StaticOobeScreenId OobeScreen::SCREEN_CONFIRM_PASSWORD;
constexpr StaticOobeScreenId OobeScreen::SCREEN_FATAL_ERROR;
constexpr StaticOobeScreenId
    OobeScreen::SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_SPECIAL_LOGIN;
constexpr StaticOobeScreenId OobeScreen::SCREEN_SPECIAL_OOBE;
constexpr StaticOobeScreenId OobeScreen::SCREEN_ENABLE_ADB_SIDELOADING;
constexpr StaticOobeScreenId OobeScreen::SCREEN_TEST_NO_WINDOW;
constexpr StaticOobeScreenId OobeScreen::SCREEN_UNKNOWN;

}  // namespace chromeos
