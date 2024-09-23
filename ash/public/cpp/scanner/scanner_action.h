// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_

#include <string>
#include <string_view>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "base/types/expected.h"
#include "url/gurl.h"

namespace ash {

// Instructs the system to open the given url.
struct OpenUrlCommand {
  GURL url;
};

// A command is an operation that can be completed on the system.
using ScannerCommand = std::variant<OpenUrlCommand>;

// This holds a particular action the user can complete in a ScannerSession. An
// action is a single command that can be applied to the system.
struct ASH_PUBLIC_EXPORT ScannerAction {
  ScannerAction(std::string_view display_name, const ScannerCommand& command);
  ScannerAction(const ScannerAction& rhs);
  ~ScannerAction();

  std::string display_name;
  ScannerCommand command;
};

// Holds the response returned from the Scanner service. This may be a list of
// 0 or more actions, or an error state.
using ScannerActionsResponse =
    base::expected<std::vector<ScannerAction>, ScannerError>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
