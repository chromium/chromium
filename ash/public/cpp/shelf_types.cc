// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_types.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"

namespace ash {

namespace {

// A delimiter used to serialize the ShelfID string pair as a single string.
constexpr char kDelimiter[] = "|";

}  // namespace

std::ostream& operator<<(std::ostream& out, ShelfAlignment alignment) {
  switch (alignment) {
    case ShelfAlignment::kBottom:
      return out << "Bottom";
    case ShelfAlignment::kLeft:
      return out << "Left";
    case ShelfAlignment::kRight:
      return out << "Right";
    case ShelfAlignment::kBottomLocked:
      return out << "BottomLocked";
  }
}

std::ostream& operator<<(std::ostream& out, ShelfAutoHideState state) {
  switch (state) {
    case SHELF_AUTO_HIDE_SHOWN:
      return out << "SHOWN";
    case SHELF_AUTO_HIDE_HIDDEN:
      return out << "HIDDEN";
  }
}

std::ostream& operator<<(std::ostream& out, ShelfBackgroundType type) {
  switch (type) {
    case ShelfBackgroundType::kDefaultBg:
      return out << "DefaultBg";
    case ShelfBackgroundType::kMaximized:
      return out << "Maximized";
    case ShelfBackgroundType::kHomeLauncher:
      return out << "HomeLauncher";
    case ShelfBackgroundType::kOobe:
      return out << "Oobe";
    case ShelfBackgroundType::kLogin:
      return out << "Login";
    case ShelfBackgroundType::kLoginNonBlurredWallpaper:
      return out << "LoginNonBlurredWallpaper";
    case ShelfBackgroundType::kOverview:
      return out << "Overview";
    case ShelfBackgroundType::kInApp:
      return out << "InApp";
  }
}

bool IsValidShelfItemType(int64_t type) {
  switch (type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_APP:
    case TYPE_UNPINNED_BROWSER_SHORTCUT:
    case TYPE_DIALOG:
    case TYPE_UNDEFINED:
      return true;
  }
  return false;
}

bool IsPinnedShelfItemType(ShelfItemType type) {
  switch (type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
      return true;
    case TYPE_APP:
    case TYPE_UNPINNED_BROWSER_SHORTCUT:
    case TYPE_DIALOG:
    case TYPE_UNDEFINED:
      return false;
  }
  NOTREACHED();
}

bool SamePinState(ShelfItemType a, ShelfItemType b) {
  if (!IsPinnedShelfItemType(a) && a != TYPE_APP)
    return false;
  if (!IsPinnedShelfItemType(b) && b != TYPE_APP)
    return false;
  const bool a_unpinned = (a == TYPE_APP);
  const bool b_unpinned = (b == TYPE_APP);
  return a_unpinned == b_unpinned;
}

ShelfID::ShelfID(const std::string& app_id, const std::string& launch_id)
    : app_id(app_id), launch_id(launch_id) {
  DCHECK(launch_id.empty() || !app_id.empty()) << "launch ids require app ids.";
}

ShelfID::~ShelfID() = default;

ShelfID::ShelfID(const ShelfID& other) = default;

ShelfID::ShelfID(ShelfID&& other) = default;

ShelfID& ShelfID::operator=(const ShelfID& other) = default;

bool ShelfID::operator==(const ShelfID& other) const {
  return app_id == other.app_id && launch_id == other.launch_id;
}

bool ShelfID::operator!=(const ShelfID& other) const {
  return !(*this == other);
}

bool ShelfID::operator<(const ShelfID& other) const {
  return app_id < other.app_id ||
         (app_id == other.app_id && launch_id < other.launch_id);
}

bool ShelfID::IsNull() const {
  return app_id.empty() && launch_id.empty();
}

std::string ShelfID::Serialize() const {
  DCHECK(!base::Contains(app_id, kDelimiter)) << "Invalid ShelfID";
  DCHECK(!base::Contains(launch_id, kDelimiter)) << "Invalid ShelfID";
  return app_id + kDelimiter + launch_id;
}

// static
ShelfID ShelfID::Deserialize(const std::string* string) {
  if (!string)
    return ShelfID();
  std::vector<std::string> components = base::SplitString(
      *string, kDelimiter, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_EQ(2u, components.size()) << "ShelfID serialized incorrectly.";
  return ShelfID(components[0], components[1]);
}

std::ostream& operator<<(std::ostream& o, const ShelfID& id) {
  return o << "app_id:" << id.app_id << ", launch_id:" << id.launch_id;
}

}  // namespace ash
