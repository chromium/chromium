// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EDUSUMER_GRADUATION_PREFS_H_
#define ASH_EDUSUMER_GRADUATION_PREFS_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace ash::graduation_prefs {

ASH_EXPORT void RegisterProfilePrefs(PrefRegistrySimple* registry);

}

#endif  // ASH_EDUSUMER_GRADUATION_PREFS_H_
