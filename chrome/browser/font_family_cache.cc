// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/font_family_cache.h"

#include <stddef.h>

#include <map>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/font_pref_change_notifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_font_webkit_names.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

// Identifies the user data on the profile.
const char kFontFamilyCacheKey[] = "FontFamilyCacheKey";

FontFamilyCache::FontFamilyCache(Profile* profile)
    : prefs_(profile->GetPrefs()) {
  font_change_registrar_.Register(
      FontPrefChangeNotifierFactory::GetForProfile(profile),
      base::BindRepeating(&FontFamilyCache::OnPrefsChanged,
                          base::Unretained(this)));
}

FontFamilyCache::~FontFamilyCache() = default;

void FontFamilyCache::FillFontFamilyMap(
    Profile* profile,
    const char* map_name,
    blink::web_pref::ScriptFontFamilyMap* map) {
  FontFamilyCache* cache =
      static_cast<FontFamilyCache*>(profile->GetUserData(&kFontFamilyCacheKey));
  if (!cache) {
    cache = new FontFamilyCache(profile);
    profile->SetUserData(&kFontFamilyCacheKey, base::WrapUnique(cache));
  }

  cache->FillFontFamilyMap(map_name, map);
}

void FontFamilyCache::FillFontFamilyMap(
    const char* map_name,
    blink::web_pref::ScriptFontFamilyMap* map) {
  // TODO(falken): Get rid of the brute-force scan over possible
  // (font family / script) combinations - see http://crbug.com/308095.
  for (size_t i = 0; i < prefs::kWebKitScriptsForFontFamilyMapsLength; ++i) {
    const char* script = prefs::kWebKitScriptsForFontFamilyMaps[i];
    std::u16string result = FetchAndCacheFont(script, map_name);
    if (!result.empty())
      (*map)[script] = result;
  }
}

std::u16string FontFamilyCache::FetchFont(const char* script,
                                          const char* map_name) {
  std::string pref_name = base::StringPrintf("%s.%s", map_name, script);
  std::string font = prefs_->GetString(pref_name.c_str());
  std::u16string font16 = base::UTF8ToUTF16(font);

  // Lazily constructs the map if it doesn't already exist.
  ScriptFontMap& map = font_family_map_[map_name];
  map[script] = font16;
  return font16;
}

std::u16string FontFamilyCache::FetchAndCacheFont(const char* script,
                                                  const char* map_name) {
  FontFamilyMap::const_iterator it = font_family_map_.find(map_name);
  if (it != font_family_map_.end()) {
    auto it2 = it->second.find(script);
    if (it2 != it->second.end())
      return it2->second;
  }

  return FetchFont(script, map_name);
}

// There are ~1000 entries in the cache. Avoid unnecessary object construction,
// including std::string.
void FontFamilyCache::OnPrefsChanged(const std::string& pref_name) {
  const size_t delimiter_length = 1;
  const char delimiter = '.';
  for (auto& it : font_family_map_) {
    const char* map_name = it.first;
    size_t map_name_length = strlen(map_name);

    // If the map name doesn't match, move on.
    if (pref_name.compare(0, map_name_length, map_name) != 0)
      continue;

    ScriptFontMap& map = it.second;
    for (auto it2 = map.begin(); it2 != map.end(); ++it2) {
      const char* script = it2->first;
      size_t script_length = strlen(script);

      // If the length doesn't match, move on.
      if (pref_name.size() !=
          map_name_length + script_length + delimiter_length)
        continue;

      // If the script doesn't match, move on.
      if (pref_name.compare(
              map_name_length + delimiter_length, script_length, script) != 0)
        continue;

      // If the delimiter doesn't match, move on.
      if (pref_name[map_name_length] != delimiter)
        continue;

      // Clear the cache.
      map.erase(it2);
      break;
    }
  }
}
