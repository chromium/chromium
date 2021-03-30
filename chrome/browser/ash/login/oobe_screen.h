// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_SCREEN_H_

#include <iosfwd>
#include <string>

namespace chromeos {

// Lists the priority of the OOBE screens with the highest priority at the top
// and the lowest priority at the bottom. This is used to check if screen
// transition is allowed as only higher or equal priority screen replaces the
// current screen.
enum OobeScreenPriority {
  SCREEN_DEVICE_DISABLED = 1,
  SCREEN_RESET,
  SCREEN_HARDWARE_ERROR,
  SCREEN_DEVICE_DEVELOPER_MODIFICATION,
  SCREEN_UPDATE_REQUIRED,
  DEFAULT
};

struct StaticOobeScreenId;

// Identifiers an OOBE screen.
struct OobeScreenId {
  // Create an identifier from a string.
  explicit OobeScreenId(const std::string& id);
  // Create an identifier from a statically created identifier. This is implicit
  // to make StaticOobeScreenId act more like OobeScreenId.
  OobeScreenId(const StaticOobeScreenId& id);

  // Name of the screen.
  std::string name;

  bool operator==(const OobeScreenId& rhs) const;
  bool operator!=(const OobeScreenId& rhs) const;
  bool operator<(const OobeScreenId& rhs) const;
  friend std::ostream& operator<<(std::ostream& stream, const OobeScreenId& id);
};

// A static identifier. An OOBE screen often statically expresses its ID in
// code. Chrome-style bans static destructors so use a const char* to point to
// the data in the binary instead of std::string.
struct StaticOobeScreenId {
  const char* name;

  OobeScreenId AsId() const;
};

struct OobeScreen {
  constexpr static StaticOobeScreenId
      SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED{"supervised-user-creation"};
  constexpr static StaticOobeScreenId SCREEN_CONFIRM_PASSWORD{
      "saml-confirm-password"};

  constexpr static StaticOobeScreenId SCREEN_UNKNOWN{"unknown"};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::OobeScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_SCREEN_H_
