// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_suggestion_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/selection/selection_suggestion.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

SelectionSuggestionTool::SelectionSuggestionTool(tabs::TabInterface& tab)
    : tab_(tab) {
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(&tab_.get())) {
    suggestion_service->RegisterTool(this);
  }
}

SelectionSuggestionTool::~SelectionSuggestionTool() {
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(&tab_.get())) {
    suggestion_service->UnregisterTool(this);
  }
}

SelectionSuggestionTool::ToolId SelectionSuggestionTool::GetToolId() const {
  return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
}

std::vector<std::string> SelectionSuggestionTool::GetCapabilities() const {
  const std::string tools_override =
      features::kGlicSelectionSuggestionsCapabilities.Get();
  if (tools_override !=
      features::kGlicSelectionSuggestionsCapabilities.default_value) {
    return base::SplitString(tools_override, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  }

  Profile* const profile = tab_->GetProfile();
  if (!profile || !profile->GetPrefs()) {
    return {};
  }
  const base::ListValue& pref_value = profile->GetPrefs()->GetList(
      contextual_cueing::prefs::kZeroStateSuggestionsSupportedTools);
  std::vector<std::string> capabilities;
  capabilities.reserve(pref_value.size());
  for (const base::Value& value : pref_value) {
    if (const std::string* const str = value.GetIfString()) {
      capabilities.push_back(*str);
    }
  }
  return capabilities;
}

std::unique_ptr<::selection::Suggestion>
SelectionSuggestionTool::CreateSuggestion(
    const ::selection::AreaOfInterest& processed_area,
    const optimization_guide::proto::SmartSelectionSuggestion&
        server_suggestion) {
  if (server_suggestion.label().empty()) {
    return nullptr;
  }
  return std::make_unique<SelectionSuggestion>(
      *tab_, base::UTF8ToUTF16(server_suggestion.label()),
      server_suggestion.label());
}

}  // namespace glic
