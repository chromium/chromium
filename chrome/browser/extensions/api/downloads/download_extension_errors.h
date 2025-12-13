// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOAD_EXTENSION_ERRORS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOAD_EXTENSION_ERRORS_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace download_extension_errors {

// Errors that can be returned through chrome.runtime.lastError.message.
inline constexpr char kEmptyFile[] = "Filename not yet determined";
inline constexpr char kFileAlreadyDeleted[] = "Download file already deleted";
inline constexpr char kFileNotRemoved[] = "Unable to remove file";
inline constexpr char kIconNotFound[] = "Icon not found";
inline constexpr char kInvalidDangerType[] = "Invalid danger type";
inline constexpr char kInvalidFilename[] = "Invalid filename";
inline constexpr char kInvalidFilter[] = "Invalid query filter";
inline constexpr char kInvalidHeaderName[] = "Invalid request header name";
inline constexpr char kInvalidHeaderUnsafe[] = "Unsafe request header name";
inline constexpr char kInvalidHeaderValue[] = "Invalid request header value";
inline constexpr char kInvalidId[] = "Invalid downloadId";
inline constexpr char kInvalidOrderBy[] = "Invalid orderBy field";
inline constexpr char kInvalidQueryLimit[] = "Invalid query limit";
inline constexpr char kInvalidState[] = "Invalid state";
inline constexpr char kInvalidURL[] = "Invalid URL";
inline constexpr char kInvisibleContext[] =
    "Javascript execution context is not visible (tab, window, popup bubble)";
inline constexpr char kNotComplete[] = "Download must be complete";
inline constexpr char kNotDangerous[] = "Download must be dangerous";
inline constexpr char kNotInProgress[] = "Download must be in progress";
inline constexpr char kNotResumable[] = "DownloadItem.canResume must be true";
inline constexpr char kOpenPermission[] =
    "The \"downloads.open\" permission is required";
inline constexpr char kShelfDisabled[] =
    "Another extension has disabled the shelf";
inline constexpr char kShelfPermission[] =
    "downloads.setShelfEnabled requires the "
    "\"downloads.shelf\" permission";
inline constexpr char kTooManyListeners[] =
    "Each extension may have at most one onDeterminingFilename listener "
    "between all of its renderer execution contexts.";
inline constexpr char kUiDisabled[] =
    "Another extension has disabled the download UI";
inline constexpr char kUiPermission[] =
    "downloads.setUiOptions requires the \"downloads.ui\" permission";
inline constexpr char kUnexpectedDeterminer[] =
    "Unexpected determineFilename call";
inline constexpr char kUserGesture[] = "User gesture required";

}  // namespace download_extension_errors

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOAD_EXTENSION_ERRORS_H_
