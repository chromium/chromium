// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/util.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

// The directory within the cryptohome to save ranking state into.
constexpr char kRankerStateDirectory[] = "launcher_ranking/";

}  // namespace

base::FilePath RankerStateDirectory(Profile* profile) {
  return profile->GetPath().AppendASCII(kRankerStateDirectory);
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
