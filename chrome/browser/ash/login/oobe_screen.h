// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_SCREEN_H_

#include <iosfwd>
#include <optional>
#include <string>

namespace ash {

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
  // TODO(https://crbug.com/1312880): Remove this.
  explicit OobeScreenId(const std::string& id);

  // Create an identifier from a statically created identifier. This is implicit
  // to make StaticOobeScreenId act more like OobeScreenId.
  OobeScreenId(const StaticOobeScreenId& id);

  std::string name;
  std::string external_api_prefix;

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
  const char* external_api_prefix = nullptr;

  OobeScreenId AsId() const;
};

struct ScreenSummary {
  ScreenSummary();
  ~ScreenSummary();
  ScreenSummary(const ScreenSummary& summary);

  StaticOobeScreenId screen_id;
  std::string icon_id;
  std::string title_id;
  std::optional<std::string> subtitle_resource;
  bool is_synced;
  bool is_revisitable;
  std::optional<bool> is_completed;
};

/* Keep it as `inline constexpr` (do not add `static`) so it exists as `inline
 * variable` and have the same address in every translation unit (more at
 * https://en.cppreference.com/w/cpp/language/inline).
 **/
inline constexpr StaticOobeScreenId OOBE_SCREEN_UNKNOWN{"unknown"};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_SCREEN_H_
