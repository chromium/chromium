// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/translate_page_tool.h"

#include <optional>
#include <utility>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

content::RenderFrameHost& GetPrimaryMainFrameOfTab(tabs::TabHandle tab_handle) {
  return *tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
}

}  // namespace

TranslatePageTool::TranslatePageTool(TaskId task_id,
                                     ToolDelegate& tool_delegate,
                                     tabs::TabInterface& tab,
                                     std::string_view target_language)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab.GetHandle()),
      target_language_(target_language) {}

TranslatePageTool::~TranslatePageTool() = default;

void TranslatePageTool::Validate(ToolCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

mojom::ActionResultPtr TranslatePageTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }

  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }

  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  if (!translate_client || !translate_client->GetTranslateManager()) {
    return MakeResult(mojom::ActionResultCode::kTranslateServiceUnavailable);
  }

  if (!target_language_.empty() &&
      !translate::TranslateDownloadManager::IsSupportedLanguage(
          target_language_)) {
    return MakeResult(mojom::ActionResultCode::kTranslateUnsupportedLanguage);
  }

  return MakeOkResult();
}

void TranslatePageTool::Invoke(ToolCallback callback) {
  tabs::TabInterface* tab = tab_handle_.Get();
  CHECK(tab);

  content::WebContents* contents = tab->GetContents();
  CHECK(contents);

  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  CHECK(translate_client);

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  CHECK(translate_manager);

  // If the page is already translated to the target language, complete
  // immediately.
  const translate::LanguageState& language_state =
      translate_client->GetLanguageState();
  if (language_state.IsPageTranslated()) {
    if (target_language_.empty() ||
        language_state.current_language() == target_language_) {
      PostResponseTask(std::move(callback), MakeOkResult());
      return;
    }
  }

  invoke_callback_ = std::move(callback);
  scoped_observation_.Observe(translate_client->translate_driver());

  // Trigger translation as if requested explicitly by the user from the menu
  // (`triggered_from_menu=true`), ensuring translation is initiated
  // immediately and manual translation metrics and fallback rules apply.
  if (target_language_.empty()) {
    translate_manager->ShowTranslateUI(/*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  } else {
    translate_manager->ShowTranslateUI(/*source_code=*/std::nullopt,
                                       /*target_code=*/target_language_,
                                       /*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  }
}

void TranslatePageTool::OnPageTranslated(
    std::string_view source_lang,
    std::string_view translated_lang,
    translate::TranslateErrors error_type) {
  scoped_observation_.Reset();
  if (!invoke_callback_) {
    return;
  }

  if (error_type == translate::TranslateErrors::NONE) {
    PostResponseTask(std::move(invoke_callback_), MakeOkResult());
  } else {
    PostResponseTask(
        std::move(invoke_callback_),
        MakeResult(mojom::ActionResultCode::kTranslateServiceUnavailable));
  }
}

void TranslatePageTool::Cancel() {
  scoped_observation_.Reset();
  invoke_callback_.Reset();
}

std::string TranslatePageTool::DebugString() const {
  return absl::StrFormat("TranslatePageTool[target_language: %s]",
                         target_language_);
}

std::string TranslatePageTool::JournalEvent() const {
  if (target_language_.empty()) {
    return "TranslatePage";
  }
  return absl::StrFormat("TranslatePage[%s]", target_language_);
}

std::unique_ptr<ObservationDelayController>
TranslatePageTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  if (!tab_handle_.Get() || !tab_handle_.Get()->GetContents()) {
    return nullptr;
  }
  return std::make_unique<ObservationDelayController>(
      GetPrimaryMainFrameOfTab(tab_handle_), task_id(), journal(),
      page_stability_config);
}

void TranslatePageTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                               ToolCallback callback) const {
  task.AddTab(tab_handle_, /*stop_task_on_detach=*/true, std::move(callback));
}

tabs::TabHandle TranslatePageTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
