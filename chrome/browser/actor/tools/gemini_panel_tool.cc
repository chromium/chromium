// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/gemini_panel_tool.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace actor {

GeminiPanelTool::GeminiPanelTool(glic::InvokeWithAutoSubmitPasskey passkey,
                                 TaskId task_id,
                                 ToolDelegate& tool_delegate,
                                 ActorSurface& actor_surface,
                                 std::string prompt)
    : Tool(task_id, tool_delegate),
      action_(Action::kPrompt),
      actor_surface_handle_(actor_surface.GetHandle()),
      prompt_(std::move(prompt)),
      auto_submit_passkey_(passkey) {}

GeminiPanelTool::GeminiPanelTool(TaskId task_id,
                                 ToolDelegate& tool_delegate,
                                 ActorSurface& actor_surface)
    : Tool(task_id, tool_delegate),
      action_(Action::kClose),
      actor_surface_handle_(actor_surface.GetHandle()) {}

GeminiPanelTool::~GeminiPanelTool() = default;

void GeminiPanelTool::Validate(ToolCallback callback) {
  if (action_ == Action::kPrompt &&
      (!prompt_.has_value() || prompt_->empty())) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                                /*requires_page_stabilization=*/false,
                                "Prompt must not be empty."));
    return;
  }

  if (actor_surface_handle_.GetTabHandle() == tabs::TabHandle::Null()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kGeminiPanelNotInTab,
                   /*requires_page_stabilization=*/false,
                   "Gemini panel can only be opened on a surface in a Tab."));
    return;
  }

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          &tool_delegate().GetProfile());
  if (!glic_service) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kGeminiPanelServiceUnavailable,
                   /*requires_page_stabilization=*/false,
                   "Glic service not available."));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

mojom::ActionResultPtr GeminiPanelTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  tabs::TabInterface* tab = actor_surface_handle_.GetTabHandle().Get();
  if (!tab || !actor_surface_handle_.Get()) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }

#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (mojom::ActionResultPtr window_validation = ValidateBrowserWindow(browser);
      !IsOk(*window_validation)) {
    return window_validation;
  }
#endif

  // TODO(b/573621953): Consider whether we need to vet the current URL.

  return MakeOkResult();
}

void GeminiPanelTool::Invoke(ToolCallback callback) {
  tabs::TabInterface* tab = actor_surface_handle_.GetTabHandle().Get();
  CHECK(tab);
  CHECK(actor_surface_handle_.Get());

  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  CHECK(browser);

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          &tool_delegate().GetProfile());
  CHECK(glic_service);

  switch (action_) {
    case Action::kPrompt: {
      CHECK(prompt_.has_value());
      CHECK(auto_submit_passkey_.has_value());
      glic::GlicInvokeOptions options(
          glic::Target(*tab), glic::mojom::InvocationSource::kActorTool);
      options.prompts.push_back(*prompt_);

      auto [success_cb, error_cb] =
          base::SplitOnceCallback(std::move(callback));
      options.on_success =
          base::BindOnce(&GeminiPanelTool::OnPromptSuccess,
                         weak_ptr_factory_.GetWeakPtr(), std::move(success_cb));
      options.on_error =
          base::BindOnce(&GeminiPanelTool::OnPromptError,
                         weak_ptr_factory_.GetWeakPtr(), std::move(error_cb));

      // TODO(b/555807751): Re-consider whether we should preview before
      // submitting.
      glic_service->InvokeWithAutoSubmit(*auto_submit_passkey_,
                                         std::move(options));
      return;
    }
    case Action::kClose: {
      if (glic_service->instance_coordinator().WouldToggleClose(browser)) {
        glic_service->instance_coordinator().Toggle(
            browser, /*prevent_close=*/false,
            glic::mojom::InvocationSource::kActorTool);
      }
      PostResponseTask(std::move(callback), MakeOkResult());
      return;
    }
  }
}

void GeminiPanelTool::OnPromptSuccess(ToolCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void GeminiPanelTool::OnPromptError(ToolCallback callback,
                                    glic::GlicInvokeError error) {
  PostResponseTask(std::move(callback),
                   MakeResult(mojom::ActionResultCode::kGeminiPanelInvokeFailed,
                              /*requires_page_stabilization=*/false,
                              "Failed to open Gemini side panel."));
}

std::string GeminiPanelTool::DebugString() const {
  switch (action_) {
    case Action::kPrompt:
      return base::StrCat(
          {"PromptGeminiPanelTool[prompt=\"", prompt_.value_or(""), "\"]"});
    case Action::kClose:
      return "CloseGeminiPanelTool";
  }
}

std::string GeminiPanelTool::JournalEvent() const {
  switch (action_) {
    case Action::kPrompt:
      return "PromptGeminiPanel";
    case Action::kClose:
      return "CloseGeminiPanel";
  }
}

GURL GeminiPanelTool::JournalURL() const {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (actor_surface && actor_surface->GetWebContents()) {
    return actor_surface->GetWebContents()->GetLastCommittedURL();
  }
  return GURL();
}

std::unique_ptr<ObservationDelayController>
GeminiPanelTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

ActorSurfaceHandle GeminiPanelTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

}  // namespace actor
