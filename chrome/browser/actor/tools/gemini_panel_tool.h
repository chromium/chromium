// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GEMINI_PANEL_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GEMINI_PANEL_TOOL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_passkeys.h"

class GURL;

namespace actor {

class ActorSurface;

// Prompts or closes the Gemini side panel in Chrome for a target surface.
class GeminiPanelTool : public Tool {
 public:
  // Constructor for `PromptGeminiPanel`.
  GeminiPanelTool(glic::InvokeWithAutoSubmitPasskey passkey,
                  TaskId task_id,
                  ToolDelegate& tool_delegate,
                  ActorSurface& actor_surface,
                  std::string prompt);

  // Constructor for `CloseGeminiPanel`.
  GeminiPanelTool(TaskId task_id,
                  ToolDelegate& tool_delegate,
                  ActorSurface& actor_surface);

  ~GeminiPanelTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  GURL JournalURL() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  enum class Action {
    kPrompt,
    kClose,
  };

  void OnPromptSuccess(ToolCallback callback);
  void OnPromptError(ToolCallback callback, glic::GlicInvokeError error);

  const Action action_;
  const ActorSurfaceHandle actor_surface_handle_;
  const std::optional<std::string> prompt_;
  const std::optional<glic::InvokeWithAutoSubmitPasskey> auto_submit_passkey_;

  base::WeakPtrFactory<GeminiPanelTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GEMINI_PANEL_TOOL_H_
