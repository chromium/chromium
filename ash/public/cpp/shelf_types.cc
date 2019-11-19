// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_types.h"

#include "base/logging.h"
#include "base/strings/string_split.h"

namespace ash {

namespace {

// A delimiter used to serialize the ShelfID string pair as a single string.
constexpr char kDelimiter[] = "|";

}  // namespace

bool IsValidShelfItemType(int64_t type) {
  return type == TYPE_PINNED_APP || type == TYPE_BROWSER_SHORTCUT ||
         type == TYPE_APP || type == TYPE_DIALOG || type == TYPE_UNDEFINED;
}

bool SamePinState(ShelfItemType a, ShelfItemType b) {
  if ((a != TYPE_PINNED_APP && a != TYPE_APP && a != TYPE_BROWSER_SHORTCUT) ||
      (b != TYPE_PINNED_APP && b != TYPE_APP && b != TYPE_BROWSER_SHORTCUT)) {
    return false;
  }
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
  DCHECK_EQ(std::string::npos, app_id.find(kDelimiter)) << "Invalid ShelfID";
  DCHECK_EQ(std::string::npos, launch_id.find(kDelimiter)) << "Invalid ShelfID";
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
