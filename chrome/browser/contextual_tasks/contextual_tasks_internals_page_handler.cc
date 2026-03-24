// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_internals_page_handler.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/url_constants.h"

ContextualTasksInternalsPageHandler::ContextualTasksInternalsPageHandler(
    Profile* profile,
    contextual_tasks::ContextualTasksContextService* context_service,
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    mojo::PendingReceiver<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPageHandler>
        receiver,
    mojo::PendingRemote<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPage> page)
    : profile_(profile),
      context_service_(context_service),
      optimization_guide_logger_(
          optimization_guide_keyed_service->GetOptimizationGuideLogger()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  if (optimization_guide_logger_) {
    optimization_guide_logger_->AddObserver(this);
  }
}

ContextualTasksInternalsPageHandler::~ContextualTasksInternalsPageHandler() {
  if (optimization_guide_logger_) {
    optimization_guide_logger_->RemoveObserver(this);
  }
}

void ContextualTasksInternalsPageHandler::GetRelevantContext(
    contextual_tasks_internals::mojom::GetRelevantContextRequestPtr request,
    GetRelevantContextCallback callback) {
  if (!context_service_) {
    std::move(callback).Run(
        contextual_tasks_internals::mojom::GetRelevantContextResponse::New());
    return;
  }

  context_service_->GetRelevantTabsForQuery(
      {
          .tab_selection_mode = request->tab_selection_mode,
          .min_model_score = request->min_model_score,
      },
      request->query,
      /*explicit_urls=*/{},
      base::BindOnce(
          [](GetRelevantContextCallback callback,
             std::vector<content::WebContents*> relevant_tabs) {
            auto result = contextual_tasks_internals::mojom::
                GetRelevantContextResponse::New();
            for (content::WebContents* web_contents : relevant_tabs) {
              auto tab = contextual_tasks_internals::mojom::Tab::New();
              tab->title = base::UTF16ToUTF8(web_contents->GetTitle());
              tab->url = web_contents->GetLastCommittedURL();
              result->relevant_tabs.push_back(std::move(tab));
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

void ContextualTasksInternalsPageHandler::SetForcedEmbeddedPageHost(
    const GURL& host) {
  // Extract just the host portion, e.g. "test.google.com" from the URL.
  // If the passed URL is empty/invalid, this clears the override.
  contextual_tasks::SetForcedEmbeddedPageHostOverride(std::string(host.host()));
}

void ContextualTasksInternalsPageHandler::GetForcedEmbeddedPageHost(
    GetForcedEmbeddedPageHostCallback callback) {
  std::string host = contextual_tasks::GetForcedEmbeddedPageHost();
  if (host.empty()) {
    std::move(callback).Run(GURL());
  } else {
    // Wrap the string host into a valid URL so it can be passed via Mojo.
    std::move(callback).Run(GURL(base::StrCat(
        {url::kHttpsScheme, url::kStandardSchemeSeparator, host})));
  }
}

void ContextualTasksInternalsPageHandler::OnLogMessageAdded(
    base::Time event_time,
    optimization_guide_common::mojom::LogSource log_source,
    const std::string& source_file,
    int source_line,
    const std::string& message) {
  if (page_ && log_source == optimization_guide_common::mojom::LogSource::
                                 CONTEXTUAL_TASKS_CONTEXT) {
    page_->OnLogMessageAdded(event_time, source_file, source_line, message);
  }
}

void ContextualTasksInternalsPageHandler::GetEligibilityState(
    GetEligibilityStateCallback callback) {
  auto state = contextual_tasks_internals::mojom::EligibilityState::New();

  if (profile_) {
    auto* aim_eligibility_service =
        AimEligibilityServiceFactory::GetForProfile(profile_);
    auto* contextual_tasks_service =
        contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
            profile_);

    state->is_contextual_tasks_enabled =
        base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks);

    auto* ui_service =
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            profile_);
    if (ui_service) {
      state->is_signed_in =
          ui_service->IsSignedInToBrowserWithValidCredentials();
      state->primary_account_in_cookie_jar =
          ui_service->CookieJarContainsPrimaryAccount();
    }

    state->is_aim_allowed_by_policy =
        AimEligibilityService::IsAimAllowedByPolicy(profile_->GetPrefs());

    if (aim_eligibility_service) {
      state->is_aim_eligible = aim_eligibility_service->IsAimEligible();
      state->is_cobrowse_eligible =
          aim_eligibility_service->IsCobrowseEligible();
    }

    if (contextual_tasks_service) {
      state->is_context_sharing_enabled =
          contextual_tasks_service->GetFeatureEligibility()
              .context_sharing_enabled;
    }

    state->is_eligible =
        contextual_tasks::EntryPointEligibilityManager::IsEligible(profile_);
  }

  std::move(callback).Run(std::move(state));
}
