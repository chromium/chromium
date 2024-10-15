// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_COMMAND_H_
#define ASH_SCANNER_SCANNER_COMMAND_H_

#include <string>
#include <string_view>
#include <variant>

#include "ash/ash_export.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "url/gurl.h"

namespace ash {

// Command to open a URL in the browser.
struct ASH_EXPORT OpenUrlCommand {
  GURL url;

  explicit OpenUrlCommand(GURL url);

  OpenUrlCommand(const OpenUrlCommand&);
  OpenUrlCommand& operator=(const OpenUrlCommand&);

  ~OpenUrlCommand();
};

// Command to upload contents to Google Drive with the given MIME type,
// then open the resulting Google Workspace document.
struct ASH_EXPORT DriveUploadCommand {
  std::string title;
  std::string contents;
  std::string contents_mime_type;
  // Assumed to have static lifetime.
  std::string_view converted_mime_type;

  DriveUploadCommand(std::string title,
                     std::string contents,
                     std::string contents_mime_type,
                     std::string_view converted_mime_type);

  DriveUploadCommand(const DriveUploadCommand&);
  DriveUploadCommand& operator=(const DriveUploadCommand&);

  ~DriveUploadCommand();
};

// Holds a single command that can be applied to the system. Used as an
// intermediate step between an `ash::ScannerAction` and performing the command.
// In some cases where `ash::ScannerAction` defines a very specific action,
// the command type may be the same as the action type.
using ScannerCommand =
    std::variant<OpenUrlCommand, DriveUploadCommand, CopyToClipboardAction>;

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_COMMAND_H_
