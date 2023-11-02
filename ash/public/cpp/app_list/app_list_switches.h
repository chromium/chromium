// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_SWITCHES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_SWITCHES_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
namespace switches {

// Please keep these flags sorted (but keep enable/disable pairs together).
ASH_PUBLIC_EXPORT extern const char kEnableCrOSActionRecorder[];
ASH_PUBLIC_EXPORT extern const char kCrOSActionRecorderCopyToDownloadDir[];
ASH_PUBLIC_EXPORT extern const char kCrOSActionRecorderDisabled[];
ASH_PUBLIC_EXPORT extern const char kCrOSActionRecorderStructuredDisabled[];
ASH_PUBLIC_EXPORT extern const char kCrOSActionRecorderWithHash[];
ASH_PUBLIC_EXPORT extern const char kCrOSActionRecorderWithoutHash[];

}  // namespace switches
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_SWITCHES_H_
