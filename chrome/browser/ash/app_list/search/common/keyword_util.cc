// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/keyword_util.h"

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace app_list {

namespace {

using ::ash::string_matching::TokenizedString;

// Return a dictionary of keywords and their associated search providers.
// Structure: { keyword: [SearchProviders] }
KeywordToProvidersMap MakeMap() {
  KeywordToProvidersMap keyword_map(
      {{u"assistant", {ProviderType::kAssistantText}},
       {u"help", {ProviderType::kHelpApp}},
       {u"explore", {ProviderType::kHelpApp}},
       {u"shortcut", {ProviderType::kKeyboardShortcut}},
       {u"keyboard", {ProviderType::kKeyboardShortcut}},
       {u"settings", {ProviderType::kOsSettings}},
       {u"personalization", {ProviderType::kPersonalization}},
       {u"drive", {ProviderType::kDriveSearch}},
       {u"file", {ProviderType::kDriveSearch, ProviderType::kFileSearch}},
       {u"app",
        {ProviderType::kInstalledApp, ProviderType::kArcAppShortcut,
         ProviderType::kPlayStoreApp}},
       {u"android",
        {ProviderType::kArcAppShortcut, ProviderType::kPlayStoreApp}},
       {u"game", {ProviderType::kGames}},
       {u"gaming", {ProviderType::kGames}},
       {u"google", {ProviderType::kOmnibox}},
       {u"web", {ProviderType::kOmnibox}},
       {u"search", {ProviderType::kOmnibox}}});

  return keyword_map;
}

}  // namespace

KeywordToProvidersPairs ExtractKeyword(const std::u16string& query) {
  // Given the user query, process into a tokenized string and
  // check if keyword exists as one of the tokens.

  TokenizedString tokenized_string(query, TokenizedString::Mode::kWords);
  KeywordToProvidersMap keyword_map = MakeMap();
  KeywordToProvidersPairs extracted_keywords_to_providers = {};

  for (const std::u16string& token : tokenized_string.tokens()) {
    const auto& keyword_to_providers_pair = keyword_map.find(token);

    if (keyword_to_providers_pair != keyword_map.end()) {
      extracted_keywords_to_providers.emplace_back(
          keyword_to_providers_pair->first, keyword_to_providers_pair->second);
    }
  }

  return extracted_keywords_to_providers;
}

}  // namespace app_list
