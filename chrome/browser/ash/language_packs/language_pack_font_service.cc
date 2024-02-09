// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/language_packs/language_pack_font_service.h"

#include <string>
#include <string_view>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"

namespace ash::language_packs {

LanguagePackFontService::LanguagePackFontService(PrefService* prefs)
    : prefs_(CHECK_DEREF(prefs)) {
  pref_accept_language_.Init(
      language::prefs::kPreferredLanguages, &*prefs_,
      base::BindRepeating(&LanguagePackFontService::InstallFontDlcs,
                          weak_factory_.GetWeakPtr()));
}

LanguagePackFontService::~LanguagePackFontService() = default;

base::flat_set<std::string>
LanguagePackFontService::GetLanguagePacksForAcceptLanguage() {
  for (std::string& locale :
       base::SplitString(pref_accept_language_.GetValue(), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (language::ExtractBaseLanguage(locale) == "ja") {
      return {"ja"};
    }
  }

  return {};
}

void LanguagePackFontService::InstallFontDlcs() {
  for (std::string& language_pack : GetLanguagePacksForAcceptLanguage()) {
    LanguagePackManager::InstallPack(kFontsFeatureId, language_pack,
                                     base::DoNothing());
  }
}

}  // namespace ash::language_packs
