// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
#define ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_

#include <string>
#include <variant>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "base/types/expected.h"

namespace ash {

// Opens the browser to the Google Calendar event creation page, with some
// fields pre-set.
struct ASH_PUBLIC_EXPORT NewCalendarEventAction {
  std::string title;

  explicit NewCalendarEventAction(std::string title);

  NewCalendarEventAction(const NewCalendarEventAction&);
  NewCalendarEventAction& operator=(const NewCalendarEventAction&);

  ~NewCalendarEventAction();
};

// Opens the browser to the Google Contacts contact creation page, with some
// fields pre-set.
struct ASH_PUBLIC_EXPORT NewContactAction {
  std::string given_name;

  explicit NewContactAction(std::string given_name);

  NewContactAction(const NewContactAction&);
  NewContactAction& operator=(const NewContactAction&);

  ~NewContactAction();
};

// Holds a particular action the user can complete in a ScannerSession,
// equivalently a single command that can be applied to the system.
using ScannerAction = std::variant<NewCalendarEventAction, NewContactAction>;

// Holds the response returned from the Scanner service. This may be a list of
// 0 or more actions, or an error state.
using ScannerActionsResponse =
    base::expected<std::vector<ScannerAction>, ScannerError>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
