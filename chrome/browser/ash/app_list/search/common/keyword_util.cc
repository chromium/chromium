// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/keyword_util.h"

#include "base/containers/flat_map.h"

namespace app_list {

namespace {

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

std::vector<std::string> TokenizeQuery(const std::u16string& query) {
  // TODO(b/262623111): Implement function to tokenize user query into
  // individual tokens.
  return std::vector<std::string>();
}

KeywordToProvidersPair ExtractKeyword(const std::u16string& query) {
  // TODO(b/262623111): Implement function to identify and extract the keywords
  // from list of tokens.

  // Implementation: Given the above dictionary of keywords, iterate through
  // each key-value pair and check if the keyword exists in the query string. If
  // a keyword exists, then return the keyword and its associated Search
  // Provider.

  for (KeywordToProvidersPair& keyword_to_providers : MakeMap()) {
    if (query.find(keyword_to_providers.first) != std::string::npos) {
      return keyword_to_providers;
    }
  }

  return KeywordToProvidersPair();
}

}  // namespace app_list
