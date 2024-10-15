// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_command.h"

#include <string>
#include <string_view>
#include <utility>

#include "url/gurl.h"

namespace ash {

OpenUrlCommand::OpenUrlCommand(GURL url) : url(std::move(url)) {}

OpenUrlCommand::OpenUrlCommand(const OpenUrlCommand&) = default;
OpenUrlCommand& OpenUrlCommand::operator=(const OpenUrlCommand&) = default;

OpenUrlCommand::~OpenUrlCommand() = default;

DriveUploadCommand::DriveUploadCommand(std::string title,
                                       std::string contents,
                                       std::string contents_mime_type,
                                       std::string_view converted_mime_type)
    : title(std::move(title)),
      contents(std::move(contents)),
      contents_mime_type(std::move(contents_mime_type)),
      converted_mime_type(std::move(converted_mime_type)) {}

DriveUploadCommand::DriveUploadCommand(const DriveUploadCommand&) = default;
DriveUploadCommand& DriveUploadCommand::operator=(const DriveUploadCommand&) =
    default;

DriveUploadCommand::~DriveUploadCommand() = default;

}  // namespace ash
