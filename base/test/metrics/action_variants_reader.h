// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_METRICS_ACTION_VARIANTS_READER_H_
#define BASE_TEST_METRICS_ACTION_VARIANTS_READER_H_

#include <map>
#include <string>
#include <vector>

namespace base::test {

// A map from a variant name to its summary.
// Variant name is substituted in the action name in place of the {token}.
// Each variant has a summary
// <action name="Bookmarks.Opened{Bookmarks_Opened_Type}">
//   <owner>chrome-collections@google.com</owner>
//   <description>""</description>
//   <token key="Bookmarks_Opened_Type">
//     <variant name="" summary="aggregated"/>
//     <variant name=".AccountStorage"
//         summary="Bookmark opened from account storage."/>
//     <variant name=".LocalStorage"
//         summary="Bookmark opened from local storage."/>
//     <variant name=".LocalStorageSyncing"
//         summary="Bookmark opened from local storage."/>
//   </token>
// </action>
//
using ActionVariantsEntryMap = std::map<std::string, std::string>;

// Finds and reads the variants prefixed with `action_name` from actions.xml.
// When a non-empty separator argument is passed, then any variants beginning
// with that prefix will have it omitted from their names in the result map. In
// the example action from the comment above, passing "Bookmarks.Opened"and "."
// should return 3 variants: AccountStorage, LocalStorage, LocalStorageSyncing.

// An action can have multiple <token>s, each with their own set of variants.
// This function returns a vector of maps, where each map corresponds to a
// <token> element and contains the variants for that token.
//
// Useful for when you want to verify that the set of variants associated with
// a particular action actually matches the set of expected values.
//
// Returns a vector of maps from name to summary. The vector will be empty on
// failure or if the action is not found.
std::vector<ActionVariantsEntryMap> ReadActionVariantsForAction(
    std::string_view action_name,
    std::string_view separator = "");

// ReadActionVariantsForAction() version that reads from the given XML content.
std::vector<ActionVariantsEntryMap> ReadActionVariantsForActionFromXmlString(
    std::string_view xml_content,
    std::string_view action_name,
    std::string_view separator = "");

}  // namespace base::test

#endif  // BASE_TEST_METRICS_ACTION_VARIANTS_READER_H_
