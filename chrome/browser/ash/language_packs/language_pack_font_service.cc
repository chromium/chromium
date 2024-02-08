// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/language_packs/language_pack_font_service.h"

class PrefService;

namespace ash::language_packs {

LanguagePackFontService::LanguagePackFontService(PrefService* prefs)
    : prefs_(*prefs) {}

}  // namespace ash::language_packs
