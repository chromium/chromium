// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_SUGGESTION_SERVICE_H_
#define CHROME_BROWSER_SELECTION_SUGGESTION_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/browser/selection/suggestion_tool.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace selection {

// TabFeature associated service for requesting zero-state or context-aware
// selection suggestions from registered feature tools.
class SuggestionService {
 public:
  DECLARE_USER_DATA(SuggestionService);

  explicit SuggestionService(tabs::TabInterface* tab);
  virtual ~SuggestionService();

  SuggestionService(const SuggestionService&) = delete;
  SuggestionService& operator=(const SuggestionService&) = delete;

  static SuggestionService* From(tabs::TabInterface* tab);
  static SuggestionService* FromTabWebContents(
      content::WebContents* tab_web_contents);

  // Registration interface for internal Chrome feature tools.
  void RegisterTool(SuggestionTool* tool);
  void UnregisterTool(SuggestionTool* tool);

  // Requests zero-state or context-aware suggestions from registered tools
  // for the active selection context in the tab's WebContents.
  virtual void RequestSuggestions(const AreaOfInterest& processed_area,
                                  SuggestionsCallback callback);

 private:
  struct ActiveRequest;

  void OnToolSuggestions(
      scoped_refptr<ActiveRequest> active_request,
      bool& tool_completed,
      std::vector<std::unique_ptr<Suggestion>> suggestions,
      bool complete);

  const raw_ref<tabs::TabInterface> tab_;
  std::vector<raw_ptr<SuggestionTool>> tools_;

  ui::ScopedUnownedUserData<SuggestionService> scoped_unowned_user_data_;

  base::WeakPtrFactory<SuggestionService> weak_factory_{this};
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_SUGGESTION_SERVICE_H_

