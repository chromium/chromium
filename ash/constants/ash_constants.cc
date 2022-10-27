// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_constants.h"

#define FPL FILE_PATH_LITERAL

namespace ash {

const base::FilePath::CharType kDriveCacheDirname[] = FPL("GCache");
const base::FilePath::CharType kNssCertDbPath[] = FPL(".pki/nssdb/cert9.db");
const base::FilePath::CharType kNssKeyDbPath[] = FPL(".pki/nssdb/key4.db");

const char kSwitchAccessInternalDevice[] = "internal";
const char kSwitchAccessUsbDevice[] = "usb";
const char kSwitchAccessBluetoothDevice[] = "bluetooth";
const char kSwitchAccessUnknownDevice[] = "unknown";

const char kFakeNowTimeStringInPixelTest[] = "Sun, 6 May 2018 14:30:00 CDT";

const char kDefaultAccessibilitySelectToSpeakVoiceName[] =
    "select_to_speak_system_voice";
const char kDefaultAccessibilitySelectToSpeakHighlightColor[] = "#5e9bff";
const char kDefaultAccessibilitySelectToSpeakEnhancedVoiceName[] =
    "default-wavenet";

}  // namespace ash
