// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TRANSLATE_PAGE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TRANSLATE_PAGE_TOOL_H_

#include <string>
#include <string_view>

#include "base/scoped_observation.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/common/translate_errors.h"

namespace actor {

// A tool that initiates translation on the active document of a tab.
class TranslatePageTool
    : public Tool,
      public translate::ContentTranslateDriver::TranslationObserver {
 public:
  TranslatePageTool(TaskId task_id,
                    ToolDelegate& tool_delegate,
                    tabs::TabInterface& tab,
                    std::string_view target_language);
  ~TranslatePageTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolCallback callback) override;
  void Cancel() override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  tabs::TabHandle GetTargetTab() const override;

  // translate::ContentTranslateDriver::TranslationObserver:
  void OnPageTranslated(std::string_view source_lang,
                        std::string_view translated_lang,
                        translate::TranslateErrors error_type) override;

 private:
  tabs::TabHandle tab_handle_;
  std::string target_language_;
  ToolCallback invoke_callback_;
  base::ScopedObservation<
      translate::ContentTranslateDriver,
      translate::ContentTranslateDriver::TranslationObserver>
      scoped_observation_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TRANSLATE_PAGE_TOOL_H_
