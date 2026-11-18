// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/util.h"

#include <optional>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "components/user_manager/user.h"

namespace app_list {
namespace {

// The directory within the cryptohome to save ranking state into.
constexpr char kRankerStateDirectory[] = "launcher_ranking/";

}  // namespace

base::FilePath RankerStateDirectory(const user_manager::User& user) {
  std::optional<base::FilePath> path = user.GetPath();
  CHECK(path) << "User's home directory is not yet mounted";
  return path->AppendASCII(kRankerStateDirectory);
}

std::string CategoryToString(const Category value) {
  return base::NumberToString(static_cast<int>(value));
}

Category StringToCategory(const std::string& value) {
  int number;
  base::StringToInt(value, &number);
  return static_cast<Category>(number);
}

}  // namespace app_list
