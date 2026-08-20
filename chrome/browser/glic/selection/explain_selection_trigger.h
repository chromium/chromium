// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SELECTION_TRIGGER_H_
#define CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SELECTION_TRIGGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace glic {

// Triggers direct Gemini API or Optimization Guide explanation requests for
// selected text.
class ExplainSelectionTrigger {
 public:
  using StreamUpdateCallback =
      base::RepeatingCallback<void(const std::string& markdown_output,
                                   bool is_complete,
                                   const std::string& error_message)>;

  ExplainSelectionTrigger();
  ExplainSelectionTrigger(const ExplainSelectionTrigger&) = delete;
  ExplainSelectionTrigger& operator=(const ExplainSelectionTrigger&) = delete;
  ~ExplainSelectionTrigger();

  // Initiates explanation request to the Gemini API or Optimization Guide.
  void RequestExplanation(content::WebContents* web_contents,
                          const std::string& selected_text,
                          const std::string& surrounding_text,
                          StreamUpdateCallback callback);

  // Returns true if inline fulfillment feature is enabled and a prompt template is available.
  static bool IsInlineFulfillmentSupported();

  // Returns true if optimization guide should be used instead of direct Gemini
  // API.
  static bool ShouldUseOptimizationGuide();

  // Returns the system prompt template (from feature param or private resource).
  static std::string GetPromptTemplate();

  // Formats the prompt template by substituting selected and surrounding text.
  static std::string FormatPrompt(const std::string& prompt_template,
                                  const std::string& selected_text,
                                  const std::string& surrounding_text);

  // Returns the Gemini API Key (from command line switch).
  static std::string GetGeminiApiKey();

 private:
  void SendGeminiApiRequest(content::WebContents* web_contents,
                            const std::string& formatted_prompt,
                            const std::string& selected_text,
                            StreamUpdateCallback callback);

  void SendOptimizationGuideRequest(content::WebContents* web_contents,
                                    const std::string& prompt,
                                    StreamUpdateCallback callback);

  void OnGeminiApiResponse(StreamUpdateCallback callback,
                           std::string selected_text,
                           std::optional<std::string> response_body);

  void OnOptimizationGuideResponse(
      StreamUpdateCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::TimeTicks start_time_;

  base::WeakPtrFactory<ExplainSelectionTrigger> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SELECTION_TRIGGER_H_
