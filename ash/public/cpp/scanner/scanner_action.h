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

// Creates a new Google Doc with the given title and contents, then opens the
// browser to that new Google Doc.
struct ASH_PUBLIC_EXPORT NewGoogleDocAction {
  std::string title;
  // The contents of the doc as a string representing HTML. This will be
  // converted to a Google Doc upon upload.
  // TODO: b/367870452 - Consider using the protobuf type directly to avoid a
  // large string copy.
  std::string html_contents;

  NewGoogleDocAction(std::string title, std::string html_contents);

  NewGoogleDocAction(const NewGoogleDocAction&);
  NewGoogleDocAction& operator=(const NewGoogleDocAction&);

  ~NewGoogleDocAction();
};

// Creates a new Google Sheet with the given title and contents, then opens the
// browser to that new Google Sheet.
struct ASH_PUBLIC_EXPORT NewGoogleSheetAction {
  std::string title;
  // The contents of the spreadsheet as a string representing CSV. This will be
  // converted to a Google Sheet upon upload.
  // TODO: b/367872031 - Consider using the protobuf type directly to avoid a
  // large string copy.
  std::string csv_contents;

  NewGoogleSheetAction(std::string title, std::string csv_contents);

  NewGoogleSheetAction(const NewGoogleSheetAction&);
  NewGoogleSheetAction& operator=(const NewGoogleSheetAction&);

  ~NewGoogleSheetAction();
};

struct ASH_PUBLIC_EXPORT CopyToClipboardAction {
  // The `text/plain` data to put on the clipboard.
  // The empty string is a SENTINEL VALUE which represents that no plain text
  // was provided, for example, if it is intended for an image to be put on the
  // clipboard.
  // All readers of this must explicitly handle the sentinel value.
  std::string plain_text;

  // The `text/html` data to put on the clipboard.
  // The empty string is a SENTINEL VALUE which represents that no rich text
  // was provided.
  // All readers of this must explicitly handle the sentinel value.
  std::string html_text;

  CopyToClipboardAction(std::string plain_text, std::string html_text);

  CopyToClipboardAction(const CopyToClipboardAction&);
  CopyToClipboardAction& operator=(const CopyToClipboardAction&);

  ~CopyToClipboardAction();
};

// Holds a particular action the user can complete in a ScannerSession,
// equivalently a single command that can be applied to the system.
using ScannerAction = std::variant<NewCalendarEventAction,
                                   NewContactAction,
                                   NewGoogleDocAction,
                                   NewGoogleSheetAction,
                                   CopyToClipboardAction>;

// Holds the response returned from the Scanner service. This may be a list of
// 0 or more actions, or an error state.
using ScannerActionsResponse =
    base::expected<std::vector<ScannerAction>, ScannerError>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCANNER_SCANNER_ACTION_H_
