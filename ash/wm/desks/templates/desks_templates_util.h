// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_UTIL_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_UTIL_H_

class PrefRegistrySimple;

namespace ash {

namespace desks_templates_util {

// Registers the per-profile preferences for whether desks templates are
// enabled.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

bool AreDesksTemplatesEnabled();

}  // namespace desks_templates_util
}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_UTIL_H_
