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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/compose/type_conversions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "components/compose/proto/compose_metadata.pb.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
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
      manager_(this) {
  profile_ = Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  opt_guide_ = OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);

  if (GetOptimizationGuide()) {
    std::vector<optimization_guide::proto::OptimizationType> types;
    if (ComposeEnabling::IsEnabledForProfile(profile_)) {
      types.push_back(optimization_guide::proto::OptimizationType::COMPOSE);
    }

    if (!types.empty()) {
      GetOptimizationGuide()->RegisterOptimizationTypes(types);
    }
  }
}

ChromeComposeClient::~ChromeComposeClient() = default;

void ChromeComposeClient::BindComposeDialog(
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  url::Origin origin =
      GetWebContents().GetPrimaryMainFrame()->GetLastCommittedOrigin();
  if (origin == url::Origin::Create(GURL(kComposeURL))) {
    debug_session_ =
        std::make_unique<ComposeSession>(&GetWebContents(), GetModelExecutor());
    debug_session_->Bind(std::move(handler), std::move(dialog));
    return;
  }

  sessions_.at(last_compose_field_id_)
      ->Bind(std::move(handler), std::move(dialog));
}

void ChromeComposeClient::ShowComposeDialog(
    autofill::AutofillComposeDelegate::UiEntryPoint ui_entry_point,
    const autofill::FormFieldData& trigger_field,
    std::optional<autofill::AutofillClient::PopupScreenLocation>
        popup_screen_location,
    ComposeCallback callback) {
  SaveFieldAndCreateComposeStateIfEmpty(trigger_field.global_id(),
                                        std::move(callback));
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

void ChromeComposeClient::SaveFieldAndCreateComposeStateIfEmpty(
    const autofill::FieldGlobalId& field_id,
    ComposeCallback callback) {
  last_compose_field_id_ = field_id;
  auto it = sessions_.find(last_compose_field_id_);
  if (it != sessions_.end()) {
    // Update existing session
    auto& existing_session = *it->second;
    existing_session.SetComposeResultCallback(std::move(callback));
    return;
  }

  sessions_.emplace(
      last_compose_field_id_,
      std::make_unique<ComposeSession>(&GetWebContents(), GetModelExecutor(),
                                       std::move(callback)));
}

compose::ComposeManager& ChromeComposeClient::GetManager() {
  return manager_;
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

  if (!ComposeEnabling::IsEnabledForProfile(profile_)) {
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
