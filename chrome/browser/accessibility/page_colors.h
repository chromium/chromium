// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_H_
#define CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

// Manages the page colors feature, which simulates forced colors mode
// at the browser level. This tracks the per-profile preference and
// updates WebContents when the state of Page Colors changes.
class PageColors : public KeyedService {
 public:
  explicit PageColors(PrefService* profile_prefs);
  PageColors(const PageColors&) = delete;
  PageColors& operator=(const PageColors&) = delete;

  ~PageColors() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void Init();

 private:
  friend class PageColorsFactory;

  // Handles when page colors preferences change.
  void OnPageColorsChanged();

  PrefChangeRegistrar pref_change_registrar_;

  raw_ptr<PrefService> profile_prefs_;

  base::WeakPtrFactory<PageColors> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_H_
