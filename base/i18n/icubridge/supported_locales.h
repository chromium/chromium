// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_SUPPORTED_LOCALES_H_
#define BASE_I18N_ICUBRIDGE_SUPPORTED_LOCALES_H_

#include "base/containers/flat_set.h"
#include "base/i18n/base_i18n_export.h"

namespace base::i18n {

class LanguageTag;

// Returns a thread-safe static set of all BCP47 `LanguageTag` locales currently
// supported by the loaded ICU library.
//
// The returned set:
// - Contains only locales that have corresponding display name translations
// inside the ICU data file (i.e. uloc_getDisplayName resolves successfully).
// - Recursively includes all parent tags of any supported locales (e.g., if
// "en-US" is supported, "en" is also included in the set).
// - Is cached in-memory on the first call and is safe to access from any
// thread.
BASE_I18N_EXPORT const base::flat_set<LanguageTag>& GetSupportedIcuLocales();

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_SUPPORTED_LOCALES_H_
