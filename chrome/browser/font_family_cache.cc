// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/font_family_cache.h"

#include <stddef.h>

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_view_util.h"
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
  auto it = font_family_map_.find(map_name);
  if (it == font_family_map_.end()) {
    std::vector<std::pair<std::string, std::u16string>> entries;
    for (const char* script : prefs::kWebKitScriptsForFontFamilyMaps) {
      std::u16string result = FetchFont(script, map_name);
      if (!result.empty()) {
        entries.emplace_back(script, std::move(result));
      }
    }
    // `ScriptFontFamilyMap` (`base::flat_map`) sorts and deduplicates `entries`
    // in-place (note that `kWebKitScriptsForFontFamilyMaps` is not strictly
    // sorted, e.g. "Geor" precedes "Geok", so `base::sorted_unique` is not
    // used).
    it = font_family_map_
             .emplace(map_name,
                      blink::web_pref::ScriptFontFamilyMap(std::move(entries)))
             .first;
  }
  *map = it->second;
}

std::u16string FontFamilyCache::FetchFont(const char* script,
                                          const char* map_name) {
  std::string_view map_name_view(map_name);
  std::string_view script_view(script);
  const size_t pref_name_len = map_name_view.size() + 1 + script_view.size();
  std::array<char, 64> buf;
  CHECK_LE(pref_name_len, buf.size());

  // `take_first()` returns the prefix subspan and advances `span` past it.
  base::span<char> span(buf);
  span.take_first(map_name_view.size()).copy_from(map_name_view);
  span.take_first<1u>()[0] = '.';
  span.take_first(script_view.size()).copy_from(script_view);
  std::string_view pref_name =
      base::as_string_view(base::span(buf).first(pref_name_len));

  std::string font = prefs_->GetString(pref_name);
  return base::UTF8ToUTF16(font);
}

void FontFamilyCache::OnPrefsChanged(const std::string& pref_name) {
  size_t delimiter_pos = pref_name.rfind('.');
  if (delimiter_pos == std::string::npos) {
    return;
  }

  std::string_view map_name =
      std::string_view(pref_name).substr(0, delimiter_pos);
  auto it = font_family_map_.find(map_name);
  if (it == font_family_map_.end()) {
    return;
  }

  std::string_view script =
      std::string_view(pref_name).substr(delimiter_pos + 1);
  for (const char* candidate_script : prefs::kWebKitScriptsForFontFamilyMaps) {
    if (script == candidate_script) {
      std::u16string result = FetchFont(candidate_script, it->first.c_str());
      if (result.empty()) {
        it->second.erase(script);
      } else {
        it->second[std::string(script)] = std::move(result);
      }
      return;
    }
  }
}
