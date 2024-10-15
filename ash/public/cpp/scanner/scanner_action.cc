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

NewGoogleDocAction::NewGoogleDocAction(std::string title,
                                       std::string html_contents)
    : title(std::move(title)), html_contents(std::move(html_contents)) {}

NewGoogleDocAction::NewGoogleDocAction(const NewGoogleDocAction&) = default;
NewGoogleDocAction& NewGoogleDocAction::operator=(const NewGoogleDocAction&) =
    default;
NewGoogleDocAction::~NewGoogleDocAction() = default;

NewGoogleSheetAction::NewGoogleSheetAction(std::string title,
                                           std::string csv_contents)
    : title(std::move(title)), csv_contents(std::move(csv_contents)) {}

NewGoogleSheetAction::NewGoogleSheetAction(const NewGoogleSheetAction&) =
    default;
NewGoogleSheetAction& NewGoogleSheetAction::operator=(
    const NewGoogleSheetAction&) = default;
NewGoogleSheetAction::~NewGoogleSheetAction() = default;

CopyToClipboardAction::CopyToClipboardAction(std::string plain_text,
                                             std::string html_text)
    : plain_text(std::move(plain_text)), html_text(std::move(html_text)) {}

CopyToClipboardAction::CopyToClipboardAction(const CopyToClipboardAction&) =
    default;
CopyToClipboardAction& CopyToClipboardAction::operator=(
    const CopyToClipboardAction&) = default;
CopyToClipboardAction::~CopyToClipboardAction() = default;

}  // namespace ash
