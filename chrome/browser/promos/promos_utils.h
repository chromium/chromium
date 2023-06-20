// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_
#define CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_

#include "components/pref_registry/pref_registry_syncable.h"

namespace promos_utils {
// RegisterProfilePrefs is a helper to register the synced profile prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// ShouldShowIOSPasswordPromo returns true if all conditions are met to show the
// IOS Password Promo.
bool ShouldShowIOSPasswordPromo();

// IsDirectVariantIOSPasswordPromo returns true if the user is in one of the
// "direct" variant groups (QR code promo).
bool IsDirectVariantIOSPasswordPromo();

// IsIndirectVariantIOSPasswordPromo returns true if the user is in one of the
// "indirect" variant groups (get started button).
bool IsIndirectVariantIOSPasswordPromo();
}  // namespace promos_utils

#endif  // CHROME_BROWSER_PROMOS_PROMOS_UTILS_H_
