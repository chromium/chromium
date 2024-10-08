// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/scanner/scanner_action.h"

#include <string>
#include <utility>

namespace ash {

NewCalendarEventAction::NewCalendarEventAction(std::string title)
    : title(std::move(title)) {}

NewCalendarEventAction::NewCalendarEventAction(const NewCalendarEventAction&) =
    default;
NewCalendarEventAction& NewCalendarEventAction::operator=(
    const NewCalendarEventAction&) = default;
NewCalendarEventAction::~NewCalendarEventAction() = default;

NewContactAction::NewContactAction(std::string given_name)
    : given_name(std::move(given_name)) {}

NewContactAction::NewContactAction(const NewContactAction&) = default;
NewContactAction& NewContactAction::operator=(const NewContactAction&) =
    default;
NewContactAction::~NewContactAction() = default;

}  // namespace ash
