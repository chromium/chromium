// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GEMINI_PANEL_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GEMINI_PANEL_TOOL_REQUEST_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/glic/public/glic_passkeys.h"  // nogncheck
#include "components/tabs/public/tab_interface.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Sends a prompt to the Gemini side panel in Chrome for the specified surface.
// The side panel is opened if it isn't already open.
class PromptGeminiPanelToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "PromptGeminiPanel";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "prompt_gemini_panel";
  // JSON argument key for the prompt parameter.
  static constexpr std::string_view kPromptParam = "prompt";

  PromptGeminiPanelToolRequest(glic::InvokeWithAutoSubmitPasskey passkey,
                               ActorSurfaceHandle actor_surface_handle,
                               std::string prompt);
  ~PromptGeminiPanelToolRequest() override;
  PromptGeminiPanelToolRequest(const PromptGeminiPanelToolRequest&);
  PromptGeminiPanelToolRequest& operator=(const PromptGeminiPanelToolRequest&);

  // Returns the `ToolId::kPromptGeminiPanel` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  // ToolRequest:
  GURL GetURLForJournal() const override;
  ActorSurfaceHandle GetActorSurfaceHandle() const override;
  bool RequiresUrlCheckInCurrentTab() const override;
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

  const std::string& prompt() const { return prompt_; }

 private:
  glic::InvokeWithAutoSubmitPasskey auto_submit_passkey_;
  ActorSurfaceHandle actor_surface_handle_;
  std::string prompt_;
};

// Closes the Gemini side panel in Chrome if it is currently open for the
// specified surface's browser window.
class CloseGeminiPanelToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "CloseGeminiPanel";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "close_gemini_panel";

  explicit CloseGeminiPanelToolRequest(ActorSurfaceHandle actor_surface_handle);
  ~CloseGeminiPanelToolRequest() override;
  CloseGeminiPanelToolRequest(const CloseGeminiPanelToolRequest&);
  CloseGeminiPanelToolRequest& operator=(const CloseGeminiPanelToolRequest&);

  // Returns the `ToolId::kCloseGeminiPanel` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  // ToolRequest:
  GURL GetURLForJournal() const override;
  ActorSurfaceHandle GetActorSurfaceHandle() const override;
  bool RequiresUrlCheckInCurrentTab() const override;
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  ActorSurfaceHandle actor_surface_handle_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GEMINI_PANEL_TOOL_REQUEST_H_
