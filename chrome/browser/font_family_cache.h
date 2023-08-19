// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FONT_FAMILY_CACHE_H_
#define CHROME_BROWSER_FONT_FAMILY_CACHE_H_

#include <string>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/font_pref_change_notifier.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

class PrefService;
class Profile;

FORWARD_DECLARE_TEST(FontFamilyCacheTest, Caching);

// Caches font family preferences associated with a PrefService. This class
// relies on the assumption that each concatenation of map_name + '.' + script
// is a unique string. It also relies on the assumption that the (const char*)
// keys used in both inner and outer maps are compile time constants.
// This class caches the strings necessary to update
// "blink::web_pref::ScriptFontFamilyMap". This is necessary since Chrome
// attempts to update blink::web_pref::ScriptFontFamilyMap 20000 times at
// startup. See https://crbug.com/308095.
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

  // Map from script to font.
  // Key comparison uses pointer equality.
  using ScriptFontMap = std::unordered_map<const char*, std::u16string>;

  // Map from font family to ScriptFontMap.
  // Key comparison uses pointer equality.
  using FontFamilyMap = std::unordered_map<const char*, ScriptFontMap>;

  // Checks the cache for the font. If not present, fetches the font and stores
  // the result in the cache.
  // This method needs to be very fast, because it's called ~20,000 times on a
  // fresh launch with an empty profile. It's important to avoid unnecessary
  // object construction, hence the heavy use of const char* and the minimal use
  // of std::string.
  // |script| and |map_name| must be compile time constants. Two behaviors rely
  // on this: key comparison uses pointer equality, and keys must outlive the
  // maps.
  std::u16string FetchAndCacheFont(const char* script, const char* map_name);

  // Called when font family preferences changed.
  // Invalidates the cached entry, and removes the relevant observer.
  // Note: It is safe to remove the observer from the pref change callback.
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
