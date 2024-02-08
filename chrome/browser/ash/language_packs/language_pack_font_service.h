// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_
#define CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace ash::language_packs {

class LanguagePackFontService : public KeyedService {
 public:
  explicit LanguagePackFontService(PrefService* prefs);

 private:
  // Not owned by this class
  const raw_ref<PrefService> prefs_;
};

}  // namespace ash::language_packs

#endif  // CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_
