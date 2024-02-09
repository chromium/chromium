// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_
#define CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace ash::language_packs {

// Executes preference-based DLC-font tasks:
// - When user's web language prefs changes, installs the appropriate font DLC.
class LanguagePackFontService : public KeyedService {
 public:
  explicit LanguagePackFontService(PrefService* prefs);
  ~LanguagePackFontService() override;

 private:
  base::flat_set<std::string> GetLanguagePacksForAcceptLanguage();
  void InstallFontDlcs();

  // Not owned by this class
  const raw_ref<PrefService> prefs_;

  StringPrefMember pref_accept_language_;

  base::WeakPtrFactory<LanguagePackFontService> weak_factory_{this};
};

}  // namespace ash::language_packs

#endif  // CHROME_BROWSER_ASH_LANGUAGE_PACKS_LANGUAGE_PACK_FONT_SERVICE_H_
