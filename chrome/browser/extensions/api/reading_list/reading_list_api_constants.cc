// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list/reading_list_api_constants.h"

namespace extensions::reading_list_api_constants {

// Error messages.
const char kInvalidURLError[] = "URL is not valid.";
const char kNotSupportedURLError[] = "URL is not supported.";
const char kDuplicateURLError[] = "Duplicate URL.";
const char kURLNotFoundError[] = "URL not found.";
const char kNoUpdateProvided[] =
    "At least one of `title` or `hasBeenRead` must be provided.";

}  // namespace extensions::reading_list_api_constants
