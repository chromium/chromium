// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/compose/type_conversions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

const char kComposeURL[] = "chrome://compose/";

}  // namespace

ChromeComposeClient::ChromeComposeClient(content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeComposeClient>(*web_contents),
      translate_language_provider_(new TranslateLanguageProvider()),
      compose_enabling_(translate_language_provider_.get()),
      manager_(this),
      close_page_receiver_(this) {
  profile_ = Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  opt_guide_ = OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);

  if (GetOptimizationGuide()) {
    std::vector<optimization_guide::proto::OptimizationType> types;
    if (compose_enabling_.IsEnabledForProfile(profile_)) {
      types.push_back(optimization_guide::proto::OptimizationType::COMPOSE);
    }

    if (!types.empty()) {
      GetOptimizationGuide()->RegisterOptimizationTypes(types);
    }
  }
}

ChromeComposeClient::~ChromeComposeClient() = default;

void ChromeComposeClient::BindComposeDialog(
    mojo::PendingReceiver<compose::mojom::ComposeDialogClosePageHandler>
        close_handler,
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  close_page_receiver_.reset();
  close_page_receiver_.Bind(std::move(close_handler));

  url::Origin origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (origin == url::Origin::Create(GURL(kComposeURL))) {
    debug_session_ =
        std::make_unique<ComposeSession>(&GetWebContents(), GetModelExecutor());
    debug_session_->Bind(std::move(handler), std::move(dialog));
    return;
  }
  sessions_.at(last_compose_field_id_.value())
      ->Bind(std::move(handler), std::move(dialog));
}

void ChromeComposeClient::ShowComposeDialog(
    autofill::AutofillComposeDelegate::UiEntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    std::optional<autofill::AutofillClient::PopupScreenLocation>
        popup_screen_location,
    ComposeCallback callback) {
  CreateSessionIfNeeded(trigger_field, std::move(callback));
  if (!skip_show_dialog_for_test_) {
    // The bounds given by autofill are relative to the top level frame. Here we
    // offset by the WebContents container to make up for that.
    gfx::RectF bounds_in_screen = trigger_field.bounds;
    bounds_in_screen.Offset(
        GetWebContents().GetContainerBounds().OffsetFromOrigin());
    compose_dialog_controller_ =
        chrome::ShowComposeDialog(GetWebContents(), bounds_in_screen);
  }
}

bool ChromeComposeClient::HasSession(
    const autofill::FieldGlobalId& trigger_field_id) {
  auto it = sessions_.find(trigger_field_id);
  return it != sessions_.end();
}

void ChromeComposeClient::CloseUI(compose::mojom::CloseReason reason) {
  switch (reason) {
    case compose::mojom::CloseReason::kCloseButton:
    case compose::mojom::CloseReason::kInsertButton:
      RemoveActiveSession();
      break;
  }

  if (compose_dialog_controller_) {
    compose_dialog_controller_->Close();
  }
}

void ChromeComposeClient::CreateSessionIfNeeded(
    const autofill::FormFieldData& trigger_field,
    ComposeCallback callback) {
  std::string selected_text = base::UTF16ToUTF8(trigger_field.GetSelection());
  auto it = sessions_.find(trigger_field.global_id());
  bool found = it != sessions_.end();
  if (found && !selected_text.empty()) {
    // If the user entered the compose dialog by selecting text, the existing
    // state must be cleared and replaced with the selected text as the input.
    RemoveActiveSession();
  }
  last_compose_field_id_ =
      std::make_optional<autofill::FieldGlobalId>(trigger_field.global_id());
  if (found && selected_text.empty()) {
    // Update existing session (only if session was not removed earlier).
    auto& existing_session = *it->second;
    existing_session.set_compose_callback(std::move(callback));
  } else {
    // Insert new session.
    sessions_.emplace(
        last_compose_field_id_.value(),
        std::make_unique<ComposeSession>(&GetWebContents(), GetModelExecutor(),
                                         std::move(callback)));
  }
  // Capture user-selected text as initial input.
  if (!selected_text.empty()) {
    auto& session = sessions_.at(last_compose_field_id_.value());
    session->set_initial_input(selected_text);
  }
}

void ChromeComposeClient::RemoveActiveSession() {
  if (debug_session_) {
    debug_session_.reset();
    return;
  }
  auto it = sessions_.find(last_compose_field_id_.value());
  CHECK(it != sessions_.end())
      << "Attempted to remove compose session that doesn't exist.";
  sessions_.erase(last_compose_field_id_.value());
  last_compose_field_id_.reset();
}

compose::ComposeManager& ChromeComposeClient::GetManager() {
  return manager_;
}

ComposeEnabling& ChromeComposeClient::GetComposeEnabling() {
  return compose_enabling_;
}

bool ChromeComposeClient::ShouldTriggerPopup(std::string autocomplete_attribute,
                                             autofill::FieldGlobalId field_id) {
  // TODO(b/303502029): When we make an enum for return state, check to see if
  // we have saved state for the current field, and offer the saved state
  // bubble.
  bool saved_state = !sessions_.empty();
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents());
  return compose_enabling_.ShouldTriggerPopup(autocomplete_attribute, profile_,
                                              translate_manager, saved_state);
}

bool ChromeComposeClient::ShouldTriggerContextMenu(
    content::RenderFrameHost* rfh,
    content::ContextMenuParams& params) {
  translate::TranslateManager* translate_manager =
      ChromeTranslateClient::GetManagerFromWebContents(&GetWebContents());
  return compose_enabling_.ShouldTriggerContextMenu(profile_, translate_manager,
                                                    rfh, params);
}

optimization_guide::OptimizationGuideModelExecutor*
ChromeComposeClient::GetModelExecutor() {
  return model_executor_for_test_.value_or(
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(GetWebContents().GetBrowserContext())));
}

optimization_guide::OptimizationGuideDecider*
ChromeComposeClient::GetOptimizationGuide() {
  return opt_guide_;
}

void ChromeComposeClient::SetModelExecutorForTest(
    optimization_guide::OptimizationGuideModelExecutor* model_executor) {
  model_executor_for_test_ = model_executor;
}

void ChromeComposeClient::SetSkipShowDialogForTest() {
  skip_show_dialog_for_test_ = true;
}

void ChromeComposeClient::SetOptimizationGuideForTest(
    optimization_guide::OptimizationGuideDecider* opt_guide) {
  opt_guide_ = opt_guide;
}

compose::ComposeHintDecision ChromeComposeClient::GetOptimizationGuidanceForUrl(
    const GURL& url) {
  if (!GetOptimizationGuide()) {
    return compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED;
  }

  if (!compose_enabling_.IsEnabledForProfile(profile_)) {
    return compose::ComposeHintDecision::COMPOSE_HINT_DECISION_COMPOSE_DISABLED;
  }

  optimization_guide::OptimizationMetadata metadata;

  auto opt_guide_has_hint = GetOptimizationGuide()->CanApplyOptimization(
      url, optimization_guide::proto::OptimizationType::COMPOSE, &metadata);
  if (opt_guide_has_hint !=
      optimization_guide::OptimizationGuideDecision::kTrue) {
    return compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED;
  }

  absl::optional<compose::ComposeHintMetadata> compose_metadata =
      optimization_guide::ParsedAnyMetadata<compose::ComposeHintMetadata>(
          metadata.any_metadata().value());
  if (!compose_metadata.has_value()) {
    return compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED;
  }

  return compose_metadata->decision();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
