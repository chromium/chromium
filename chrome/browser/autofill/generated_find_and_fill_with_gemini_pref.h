// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_GENERATED_FIND_AND_FILL_WITH_GEMINI_PREF_H_
#define CHROME_BROWSER_AUTOFILL_GENERATED_FIND_AND_FILL_WITH_GEMINI_PREF_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace autofill {

inline constexpr char kGeneratedFindAndFillWithGeminiPref[] =
    "generated.find_and_fill_with_gemini";

// A synthetic preference representing the effective "Find and Fill with Gemini"
// setting state in Chrome Settings.
//
// Reads and writes the backing user preference
// `personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus`.
//
// When the enterprise policy pref
// `optimization_guide::prefs::kFindAndFillWithGeminiSettings` is set to
// disabled (`kDisable` / `2`), the generated preference returns `false`
// enforced by enterprise policy and ignores write requests
// (`PREF_NOT_MODIFIABLE`). Otherwise, it delegates reading and writing directly
// to the backing user pref.
class GeneratedFindAndFillWithGeminiPref
    : public extensions::settings_private::GeneratedPref {
 public:
  explicit GeneratedFindAndFillWithGeminiPref(Profile* profile);

  GeneratedFindAndFillWithGeminiPref(
      const GeneratedFindAndFillWithGeminiPref&) = delete;
  GeneratedFindAndFillWithGeminiPref& operator=(
      const GeneratedFindAndFillWithGeminiPref&) = delete;

  ~GeneratedFindAndFillWithGeminiPref() override;

  // GeneratedPref implementation:
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  extensions::api::settings_private::PrefObject GetPrefObject() const override;

  void OnSourcePreferencesChanged();

 private:
  const raw_ref<Profile> profile_;
  PrefChangeRegistrar prefs_registrar_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_GENERATED_FIND_AND_FILL_WITH_GEMINI_PREF_H_
