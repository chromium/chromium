// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/font_family_cache.h"

#include <stddef.h>

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
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
    MapName map_name,
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
    MapName map_name,
    blink::web_pref::ScriptFontFamilyMap* map) {
  auto it = font_family_map_.find(map_name.value());
  if (it == font_family_map_.end()) {
    blink::web_pref::ScriptFontFamilyMap cached_map;
    for (const char* script : prefs::kWebKitScriptsForFontFamilyMaps) {
      std::u16string result = FetchFont(script, map_name.value());
      if (!result.empty()) {
        cached_map.emplace(script, std::move(result));
      }
    }
    it =
        font_family_map_.emplace(map_name.value(), std::move(cached_map)).first;
  }
  *map = it->second;
  CHECK(map->contains(blink::web_pref::kCommonScript));
}

std::u16string FontFamilyCache::FetchFont(const char* script,
                                          std::string_view map_name) {
  std::string_view script_view(script);
  const size_t pref_name_len = map_name.size() + 1 + script_view.size();
  std::array<char, 64> buf;
  CHECK_LE(pref_name_len, buf.size());

  // `take_first()` returns the prefix subspan and advances `span` past it.
  base::span<char> span(buf);
  span.take_first(map_name.size()).copy_from(map_name);
  span.take_first<1u>()[0] = '.';
  span.take_first(script_view.size()).copy_from(script_view);
  std::string_view pref_name =
      base::as_string_view(base::span(buf).first(pref_name_len));

  const std::string* font = &prefs_->GetString(pref_name);
  if (font->empty() && script_view == blink::web_pref::kCommonScript)
      [[unlikely]] {
    // A `kCommonScript` ("Zyyy") font preference always has a non-empty default
    // value registered by `PrefsTabHelper::RegisterProfilePrefs`, but an
    // extension could explicitly set the preference to "". Fall back to the
    // registered default value so that `ScriptFontFamilyMap` always has a
    // non-empty entry for `kCommonScript`.
    font = &prefs_->GetDefaultPrefValue(pref_name)->GetString();
  }
  return base::UTF8ToUTF16(*font);
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
      std::u16string result = FetchFont(candidate_script, it->first);
      if (result.empty()) {
        it->second.erase(candidate_script);
      } else {
        it->second[candidate_script] = std::move(result);
      }
      return;
    }
  }
}
