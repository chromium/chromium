// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_

#include <iosfwd>
#include <string>

namespace chromeos {

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
  constexpr static StaticOobeScreenId SCREEN_ACCOUNT_PICKER{"account-picker"};

  constexpr static StaticOobeScreenId SCREEN_TPM_ERROR{"tpm-error-message"};
  constexpr static StaticOobeScreenId SCREEN_PASSWORD_CHANGED{
      "password-changed"};
  constexpr static StaticOobeScreenId
      SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED{"supervised-user-creation"};
  constexpr static StaticOobeScreenId SCREEN_CONFIRM_PASSWORD{
      "confirm-password"};
  constexpr static StaticOobeScreenId SCREEN_FATAL_ERROR{"fatal-error"};
  constexpr static StaticOobeScreenId SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE{
      "ad-password-change"};

  // Special "first screen" that initiates login flow.
  constexpr static StaticOobeScreenId SCREEN_SPECIAL_LOGIN{"login"};
  // Special "first screen" that initiates full OOBE flow.
  constexpr static StaticOobeScreenId SCREEN_SPECIAL_OOBE{"oobe"};
  // Special "first screen" that initiates enabling ARC adb sideloading flow.
  constexpr static StaticOobeScreenId SCREEN_ENABLE_ADB_SIDELOADING{
      "adb-sideloading"};
  // Special test value that commands not to create any window yet.
  constexpr static StaticOobeScreenId SCREEN_TEST_NO_WINDOW{"test:nowindow"};

  constexpr static StaticOobeScreenId SCREEN_UNKNOWN{"unknown"};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_
