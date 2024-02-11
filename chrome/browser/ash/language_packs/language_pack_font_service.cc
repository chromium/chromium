// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/language_packs/language_pack_font_service.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/linux/fontconfig_util.h"

namespace ash::language_packs {

LanguagePackFontService::LanguagePackFontService(PrefService* prefs)
    : LanguagePackFontService(prefs, base::BindRepeating(&gfx::AddAppFontDir)) {
}

LanguagePackFontService::LanguagePackFontService(
    PrefService* prefs,
    base::RepeatingCallback<bool(base::FilePath)> add_font_dir)
    : prefs_(CHECK_DEREF(prefs)), add_font_dir_(std::move(add_font_dir)) {
  pref_accept_language_.Init(
      language::prefs::kPreferredLanguages, &*prefs_,
      base::BindRepeating(&LanguagePackFontService::InstallFontDlcs,
                          weak_factory_.GetWeakPtr()));

  // Add installed fonts to fontconfig.
  // The below DLC calls may race `InstallFontDlcs` if the preference is updated
  // while DLC state is being returned. In the best case, the install wins the
  // race, and we add the font to fontconfig prematurely. Otherwise, the "get
  // state" wins the race, and we enqueue another DLC installation (which should
  // instantly resolve.)
  for (std::string& language_pack : GetLanguagePacksForAcceptLanguage()) {
    LanguagePackManager::GetPackState(
        kFontsFeatureId, language_pack,
        base::BindOnce(
            &LanguagePackFontService::AddDlcFontDirsToFontConfigPackCallback,
            weak_factory_.GetWeakPtr()));
  }
}

LanguagePackFontService::~LanguagePackFontService() = default;

base::flat_set<std::string>
LanguagePackFontService::GetLanguagePacksForAcceptLanguage() {
  base::flat_set<std::string> language_packs;

  for (std::string& locale :
       base::SplitString(pref_accept_language_.GetValue(), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::string_view language = language::ExtractBaseLanguage(locale);
    if (language == "ja") {
      language_packs.insert("ja");
    }
    if (language == "ko") {
      language_packs.insert("ko");
    }
  }

  return language_packs;
}

void LanguagePackFontService::InstallFontDlcs() {
  for (std::string& language_pack : GetLanguagePacksForAcceptLanguage()) {
    LanguagePackManager::InstallPack(kFontsFeatureId, language_pack,
                                     base::DoNothing());
  }
}

void LanguagePackFontService::AddDlcFontDirsToFontConfigPackCallback(
    const PackResult& result) {
  if (result.pack_state != PackResult::StatusCode::kInstalled &&
      !result.language_code.empty()) {
    LanguagePackManager::InstallPack(kFontsFeatureId, result.language_code,
                                     base::DoNothing());
    return;
  }

  // All fontconfig methods need to be called on the "main" thread.
  // As this method is only called from a callback which should be on the "main"
  // thread, the following `CHECK` should never fail.
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  bool add_font_result = add_font_dir_.Run(base::FilePath(result.path));
  if (!add_font_result) {
    LOG(WARNING) << "Adding font for " << result.language_code << " failed";
  }
}

}  // namespace ash::language_packs
