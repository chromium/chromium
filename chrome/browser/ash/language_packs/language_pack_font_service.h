// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_
#define CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace base {
class FilePath;
}

namespace ash::language_packs {

struct PackResult;

// Executes preference-based DLC-font tasks:
// - On initialisation of this keyed service (which is created with the browser
//   context), gets the user's web language prefs and adds the fonts available
//   from DLC to fontconfig.
// - When user's web language prefs changes, installs the appropriate font DLC.
class LanguagePackFontService : public KeyedService {
 public:
  using AddFontDir = base::RepeatingCallback<bool(const base::FilePath&)>;

  explicit LanguagePackFontService(PrefService* prefs);
  // Used for injecting `gfx::AddAppFontDir` for tests. `add_font_dir` should
  // return whether the font was added. A warning will be printed to the log if
  // it was not added, which should never happen.
  explicit LanguagePackFontService(PrefService* prefs, AddFontDir add_font_dir);
  ~LanguagePackFontService() override;

 private:
  base::flat_set<std::string> GetLanguagePacksForAcceptLanguage();
  void OnAcceptLanguageChanged();
  void GetPackStateOnInitCallback(const PackResult& result);
  void InstallPackOnInitCallback(const PackResult& result);
  void AddFontDirFromPackResult(const PackResult& result);

  // Not owned by this class
  const raw_ref<PrefService> prefs_;

  StringPrefMember pref_accept_language_;

  // Used for dependency injection for tests.
  AddFontDir add_font_dir_;

  base::WeakPtrFactory<LanguagePackFontService> weak_factory_{this};
};

}  // namespace ash::language_packs

#endif  // CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_
