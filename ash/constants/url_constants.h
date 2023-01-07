// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_URL_CONSTANTS_H_
#define ASH_CONSTANTS_URL_CONSTANTS_H_

#include "base/component_export.h"

// Contains constants for known URLs for ash components. URL constants for
// ash-chrome should be in chrome/common/url_contants.h.
//
// - Keep the constants sorted by name within its section.
// - Use the same order in this header and ash_url_constants.cc.

namespace chrome {

// The URL for the "Learn more" link for Android Messages.
COMPONENT_EXPORT(ASH_CONSTANTS)
extern const char kAndroidMessagesLearnMoreURL[];

// The URL for additional help that is given when Linux export/import fails.
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kLinuxExportImportHelpURL[];

// The URL for the "Learn more" link in the connected devices.
COMPONENT_EXPORT(ASH_CONSTANTS) extern const char kMultiDeviceLearnMoreURL[];

}  // namespace chrome

#endif  // ASH_CONSTANTS_URL_CONSTANTS_H_
