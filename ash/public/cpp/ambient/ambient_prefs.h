// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
namespace ambient {
namespace prefs {

// A GUID for backdrop client.
ASH_PUBLIC_EXPORT extern const char kAmbientBackdropClientId[];

// Boolean pref for whether ambient mode is enabled.
ASH_PUBLIC_EXPORT extern const char kAmbientModeEnabled[];

// Integer pref for reporting metrics with the histogram
// |Ash.AmbientMode.PhotoSource|. Not displayed to the user in settings.
ASH_PUBLIC_EXPORT extern const char kAmbientModePhotoSourcePref[];

}  // namespace prefs
}  // namespace ambient
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_PREFS_H_
