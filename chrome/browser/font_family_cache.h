// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FONT_FAMILY_CACHE_H_
#define CHROME_BROWSER_FONT_FAMILY_CACHE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/font_pref_change_notifier.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

class PrefService;
class Profile;

FORWARD_DECLARE_TEST(FontFamilyCacheTest, Caching);

// Caches font family preferences associated with a PrefService.
// This class caches the blink::web_pref::ScriptFontFamilyMap instances for each
// generic font family to avoid repeated PrefService lookups and map
// reconstructions when computing WebPreferences. See
// https://crbug.com/40337107.
class FontFamilyCache : public base::SupportsUserData::Data {
 public:
  explicit FontFamilyCache(Profile* profile);

  FontFamilyCache(const FontFamilyCache&) = delete;
  FontFamilyCache& operator=(const FontFamilyCache&) = delete;

  ~FontFamilyCache() override;

  // Gets or creates the relevant FontFamilyCache, and then fills |map|.
  static void FillFontFamilyMap(Profile* profile,
                                const char* map_name,
                                blink::web_pref::ScriptFontFamilyMap* map);

  // Fills |map| with font family preferences.
  void FillFontFamilyMap(const char* map_name,
                         blink::web_pref::ScriptFontFamilyMap* map);

 protected:
  // Exposed and virtual for testing.
  // Fetches the font without checking the cache.
  virtual std::u16string FetchFont(const char* script, const char* map_name);

 private:
  FRIEND_TEST_ALL_PREFIXES(::FontFamilyCacheTest, Caching);

  // Map from font family pref prefix (e.g. "webkit.webprefs.fonts.standard")
  // to cached ScriptFontFamilyMap.
  using FontFamilyMap =
      absl::flat_hash_map<std::string, blink::web_pref::ScriptFontFamilyMap>;

  // Called when font family preferences changed.
  // Updates the cached entry if present.
  void OnPrefsChanged(const std::string& pref_name);

  // Cache of font family preferences.
  FontFamilyMap font_family_map_;

  // Weak reference.
  // Note: The lifetime of this object is tied to the lifetime of the
  // PrefService, so there is no worry about an invalid pointer.
  raw_ptr<const PrefService, AcrossTasksDanglingUntriaged> prefs_;

  // Reacts to profile font changes. |font_change_registrar_| will be
  // automatically unregistered when the FontPrefChangeNotifier is destroyed as
  // part of Profile destruction, thus ensuring safe unregistration even though
  // |this| is destroyed after the Profile destructor completes as part of
  // Profile's super class destructor ~base::SupportsUserData.
  FontPrefChangeNotifier::Registrar font_change_registrar_;
};

#endif  // CHROME_BROWSER_FONT_FAMILY_CACHE_H_
