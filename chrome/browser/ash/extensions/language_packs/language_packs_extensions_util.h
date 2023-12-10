// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_LANGUAGE_PACKS_LANGUAGE_PACKS_EXTENSIONS_UTIL_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_LANGUAGE_PACKS_LANGUAGE_PACKS_EXTENSIONS_UTIL_H_

#include "chrome/common/extensions/api/input_method_private.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"

namespace chromeos {

extensions::api::input_method_private::LanguagePackStatus
LanguagePackResultToExtensionStatus(
    const ash::language_packs::PackResult& result);

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_LANGUAGE_PACKS_LANGUAGE_PACKS_EXTENSIONS_UTIL_H_
