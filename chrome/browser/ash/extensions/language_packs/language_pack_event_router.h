// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_LANGUAGE_PACKS_LANGUAGE_PACK_EVENT_ROUTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_LANGUAGE_PACKS_LANGUAGE_PACK_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

// Event router class for language pack extension events.
// Currently instantiated in `InputMethodAPI`.
class LanguagePackEventRouter
    : public ash::language_packs::LanguagePackManager::Observer {
 public:
  explicit LanguagePackEventRouter(content::BrowserContext* context);

  LanguagePackEventRouter(const LanguagePackEventRouter&) = delete;
  LanguagePackEventRouter& operator=(const LanguagePackEventRouter&) = delete;

  ~LanguagePackEventRouter() override;

  void OnPackStateChanged(
      const ash::language_packs::PackResult& pack_result) override;

 private:
  const raw_ptr<content::BrowserContext> context_;
  base::ScopedObservation<ash::language_packs::LanguagePackManager,
                          ash::language_packs::LanguagePackManager::Observer>
      obs_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_LANGUAGE_PACKS_LANGUAGE_PACK_EVENT_ROUTER_H_
