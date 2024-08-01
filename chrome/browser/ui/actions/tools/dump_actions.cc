// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This command-line program dumps the computed values of all actiop IDs to
// stdout.

#include <algorithm>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>

#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "ui/actions/action_id.h"

#define STRINGIZE_ACTION_IDS
#include "ui/actions/action_id_macros.inc"

namespace {
// clang-format off
const char* enum_names[] = {
  ACTION_IDS
  CHROME_ACTION_IDS
};
// clang-format on
}  // namespace

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/actions/action_id_macros.inc"

int main(int argc, const char* argv[]) {
  const size_t longest_name =
      strlen(base::ranges::max(enum_names, base::ranges::less(), strlen)) + 1;

  std::cout << std::setfill(' ') << std::left;
  std::cout << std::setw(longest_name) << "ID";
  std::cout << '\n';
  std::cout << std::setfill('-') << std::right;
  std::cout << std::setw(longest_name) << ' ';
  std::cout << '\n';

  for (actions::ActionId id = actions::kActionsStart; id < kChromeActionsEnd;
       ++id) {
    std::cout << std::setfill(' ') << std::left;
    std::cout << std::setw(longest_name) << enum_names[id] << '\n';
  }

  std::cout.flush();
  return 0;
}
