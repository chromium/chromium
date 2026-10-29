// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_internals_page_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/lens_server_proto/lens_overlay_request_id.pb.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "url/url_constants.h"

namespace {

std::string ContextStateToString(
    contextual_tasks::ContextualTasksWebContentsUserData::ContextState state) {
  using ContextState =
      contextual_tasks::ContextualTasksWebContentsUserData::ContextState;
  switch (state) {
    case ContextState::kNone:
      return "kNone";
    case ContextState::kUploading:
      return "kUploading";
    case ContextState::kReady:
      return "kReady";
  }
  NOTREACHED();
}

std::string InputTypeToString(omnibox::InputType type) {
  switch (type) {
    case omnibox::INPUT_TYPE_UNSPECIFIED:
      return "INPUT_TYPE_UNSPECIFIED";
    case omnibox::INPUT_TYPE_LENS_IMAGE:
      return "INPUT_TYPE_LENS_IMAGE";
    case omnibox::INPUT_TYPE_LENS_FILE:
      return "INPUT_TYPE_LENS_FILE";
    case omnibox::INPUT_TYPE_BROWSER_TAB:
      return "INPUT_TYPE_BROWSER_TAB";
    case omnibox::INPUT_TYPE_DRIVE:
      return "INPUT_TYPE_DRIVE";
    default:
      return "INPUT_TYPE_UNKNOWN";
  }
}

std::string FormatRequestId(
    base::optional_ref<const lens::LensOverlayRequestId> request_id) {
  if (!request_id.has_value()) {
    return std::string();
  }
  return base::StrCat(
      {"ctx=", base::NumberToString(request_id->context_id()),
       " seq=", base::NumberToString(request_id->sequence_id())});
}

contextual_tasks_internals::mojom::TabContextEntryDebugPtr
BuildTabContextEntryDebug(const contextual_search::TabInfo& tab) {
  auto entry = contextual_tasks_internals::mojom::TabContextEntryDebug::New();
  entry->tab_id = tab.tab_id;
  entry->title = tab.title;
  entry->url = tab.url;
  if (!tab.context_token.is_empty()) {
    entry->context_token = tab.context_token;
  }
  entry->request_id = FormatRequestId(tab.request_id);
  entry->uploaded = tab.uploaded;
  entry->submitted = tab.submitted;
  entry->pending_delayed_upload = tab.pending_delayed_upload;
  entry->restored_from_aim = tab.restored_from_aim;
  return entry;
}

contextual_tasks_internals::mojom::InputStateDebugStatePtr
BuildInputStateDebugState(const contextual_search::InputStateModel& model) {
  auto input_debug =
      contextual_tasks_internals::mojom::InputStateDebugState::New();
  input_debug->has_valid_config = model.has_valid_config();
  input_debug->is_smart_tab_sharing_active = model.IsSmartTabSharingActive();
  input_debug->has_lens_crop = model.lens_crop().has_value();

  const omnibox::InputState& input_state = model.GetInputState();
  input_debug->active_tool =
      omnibox::GetToolModeString(input_state.active_tool);
  input_debug->active_model =
      omnibox::GetModelModeString(input_state.active_model);
  for (omnibox::ToolMode tool : input_state.allowed_tools) {
    input_debug->allowed_tools.push_back(omnibox::GetToolModeString(tool));
  }
  for (omnibox::ToolMode tool : input_state.disabled_tools) {
    input_debug->disabled_tools.push_back(omnibox::GetToolModeString(tool));
  }
  for (omnibox::ModelMode m : input_state.allowed_models) {
    input_debug->allowed_models.push_back(omnibox::GetModelModeString(m));
  }
  for (omnibox::ModelMode m : input_state.disabled_models) {
    input_debug->disabled_models.push_back(omnibox::GetModelModeString(m));
  }
  for (omnibox::InputType input_type : input_state.allowed_input_types) {
    input_debug->allowed_input_types.push_back(InputTypeToString(input_type));
  }
  for (omnibox::InputType input_type : input_state.disabled_input_types) {
    input_debug->disabled_input_types.push_back(InputTypeToString(input_type));
  }
  for (omnibox::InputType input_type : model.GetEffectiveInputTypes()) {
    input_debug->effective_input_types.push_back(InputTypeToString(input_type));
  }
  return input_debug;
}

contextual_tasks_internals::mojom::UserDataDebugStatePtr
BuildUserDataDebugState(
    const contextual_tasks::ContextualTasksWebContentsUserData* user_data,
    const ContextualSearchWebContentsHelper* helper,
    contextual_tasks::ContextualTasksService* tasks_service,
    SessionID session_tab_id) {
  auto user_data_state =
      contextual_tasks_internals::mojom::UserDataDebugState::New();
  user_data_state->has_contextual_tasks_user_data = (user_data != nullptr);
  user_data_state->has_contextual_search_helper = (helper != nullptr);

  if (helper) {
    if (helper->task_id().has_value() && helper->task_id()->is_valid()) {
      user_data_state->helper_task_id = helper->task_id()->AsLowercaseString();
    }
    for (int32_t selected_id : helper->GetSelectedTabIds()) {
      user_data_state->helper_selected_tab_ids.push_back(selected_id);
    }
  }

  if (tasks_service && session_tab_id.is_valid()) {
    std::optional<contextual_tasks::ContextualTask> resolved_task =
        tasks_service->GetContextualTaskForTab(session_tab_id);
    if (resolved_task) {
      user_data_state->service_task_id_for_tab =
          resolved_task->GetTaskId().AsLowercaseString();
    }
  }

  if (user_data) {
    if (user_data->task_id().has_value() && user_data->task_id()->is_valid()) {
      user_data_state->user_data_task_id =
          user_data->task_id()->AsLowercaseString();
    }
    if (user_data->pending_task_id().has_value() &&
        user_data->pending_task_id()->is_valid()) {
      user_data_state->pending_task_id =
          user_data->pending_task_id()->AsLowercaseString();
    }
    std::optional<base::Uuid> effective_task_id =
        user_data->GetEffectiveTaskId();
    if (effective_task_id.has_value() && effective_task_id->is_valid()) {
      user_data_state->effective_task_id =
          effective_task_id->AsLowercaseString();
      if (tasks_service) {
        std::vector<SessionID> associated_tabs =
            tasks_service->GetTabsAssociatedWithTask(*effective_task_id);
        for (SessionID tab_id : associated_tabs) {
          user_data_state->active_underline_tab_ids.push_back(tab_id.id());
        }
      }
    }

    for (const auto& [tab_handle_id, mapped_session_id] :
         user_data->tab_handle_to_session_id()) {
      auto mapping = contextual_tasks_internals::mojom::TabIdMappingDebug::New(
          tab_handle_id, mapped_session_id);
      user_data_state->tab_id_mappings.push_back(std::move(mapping));
    }

    contextual_tasks::ContextualTasksWebContentsUserData::PageSearchDebugState
        page_debug = user_data->GetPageSearchDebugState();
    user_data_state->connected_document_id = page_debug.connected_document_id;
    user_data_state->is_service_worker_port_connected =
        user_data->IsServiceWorkerPortConnected();
    user_data_state->is_handshake_complete = page_debug.is_handshake_complete;
    user_data_state->pending_search_messages_count =
        base::checked_cast<uint32_t>(page_debug.pending_search_messages_count);
    user_data_state->context_library_is_active =
        page_debug.context_library_is_active;
    user_data_state->is_lens_crop_mounted = page_debug.is_lens_crop_mounted;
    user_data_state->last_sent_context_state =
        ContextStateToString(page_debug.last_sent_context_state);
    user_data_state->has_bound_extension_frame =
        user_data->HasBoundExtensionFrame();

    const auto& frames = user_data->extension_frames();
    for (size_t i = 0; i < frames.size(); ++i) {
      user_data_state->extension_frames.push_back(
          contextual_tasks_internals::mojom::ExtensionFrameDebug::New(
              base::checked_cast<uint32_t>(i), frames[i].is_page_bound,
              frames[i].is_handshake_complete));
    }

    user_data_state->submit_pending = user_data->submit_pending();
    for (const auto& token : user_data->pending_context_uploads()) {
      user_data_state->pending_context_upload_tokens.push_back(token);
    }
    for (const auto& token : user_data->failed_context_uploads()) {
      user_data_state->failed_context_upload_tokens.push_back(token);
    }
    for (const auto& tab_info : user_data->GetSelectedTabsSnapshot()) {
      user_data_state->selected_tabs.push_back(
          BuildTabContextEntryDebug(tab_info));
    }

    if (const auto* model = user_data->input_state_model().get()) {
      user_data_state->input_state = BuildInputStateDebugState(*model);
    }
  }

  return user_data_state;
}

}  // namespace

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
          optimization_guide_keyed_service
              ? optimization_guide_keyed_service->GetOptimizationGuideLogger()
              : nullptr),
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

  contextual_tasks::TabSelectionOptions options;
  options.tab_selection_mode = request->tab_selection_mode;
  options.min_model_score = request->min_model_score;
  context_service_->GetRelevantTabsForQuery(
      options, request->query,
      /*explicit_urls=*/{},
      base::BindOnce(
          [](GetRelevantContextCallback callback,
             std::vector<base::WeakPtr<content::WebContents>> relevant_tabs) {
            auto result = contextual_tasks_internals::mojom::
                GetRelevantContextResponse::New();
            for (const auto& web_contents : relevant_tabs) {
              if (web_contents) {
                auto tab = contextual_tasks_internals::mojom::Tab::New();
                tab->title = base::UTF16ToUTF8(web_contents->GetTitle());
                tab->url = web_contents->GetLastCommittedURL();
                result->relevant_tabs.push_back(std::move(tab));
              }
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

void ContextualTasksInternalsPageHandler::SetForcedEmbeddedPageHost(
    const GURL& host) {
  if (!profile_ || profile_->IsOffTheRecord()) {
    return;
  }

  // If the passed URL is empty/invalid, this clears the override.
  if (!host.is_valid() || host.is_empty()) {
    contextual_tasks::SetForcedEmbeddedPageHostOverride(std::nullopt);
    return;
  }

  std::optional<uint16_t> port;
  if (host.has_port()) {
    port = static_cast<uint16_t>(host.EffectiveIntPort());
  }

  contextual_tasks::SetForcedEmbeddedPageHostOverride(
      contextual_tasks::HostOverride{std::string(host.host()), port});
}

void ContextualTasksInternalsPageHandler::GetForcedEmbeddedPageHost(
    GetForcedEmbeddedPageHostCallback callback) {
  std::optional<contextual_tasks::HostOverride> host =
      contextual_tasks::GetForcedEmbeddedPageHost();
  if (!host.has_value()) {
    std::move(callback).Run(GURL());
  } else {
    // Wrap the host and port into a valid URL so it can be passed via Mojo.
    std::move(callback).Run(GURL(base::StrCat(
        {url::kHttpsScheme, url::kStandardSchemeSeparator, host->ToString()})));
  }
}

void ContextualTasksInternalsPageHandler::GetEligibilityState(
    GetEligibilityStateCallback callback) {
  auto state = contextual_tasks_internals::mojom::EligibilityState::New();

  if (profile_) {
    auto* aim_eligibility_service =
        AimEligibilityServiceFactory::GetForProfile(profile_);
    state->is_contextual_tasks_enabled =
        contextual_tasks::IsContextualTasksUIEnabled();

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

    state->is_context_sharing_enabled =
        contextual_search::ContextualSearchService::IsContextSharingEnabled(
            profile_->GetPrefs());

    state->is_default_search_engine_google =
        search::DefaultSearchProviderIsGoogle(profile_);

    state->is_eligible =
        contextual_tasks::EntryPointEligibilityManager::IsEligible(profile_);
  }

  std::move(callback).Run(std::move(state));
}

void ContextualTasksInternalsPageHandler::GetSessionAndUserDataState(
    GetSessionAndUserDataStateCallback callback) {
  std::vector<contextual_tasks_internals::mojom::WebContentsDebugStatePtr>
      results;
  if (!profile_) {
    std::move(callback).Run(std::move(results));
    return;
  }

  base::flat_set<content::WebContents*> seen_web_contents;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, &results, &seen_web_contents](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile_) {
          return true;
        }
        if (auto* panel_controller =
                contextual_tasks::ContextualTasksPanelController::From(
                    browser)) {
          for (content::WebContents* panel_wc :
               panel_controller->GetPanelWebContentsList()) {
            if (!panel_wc || !seen_web_contents.insert(panel_wc).second) {
              continue;
            }
            if (auto debug_state = BuildWebContentsDebugState(
                    panel_wc, /*is_side_panel=*/true,
                    /*is_active_tab_in_window=*/false)) {
              results.push_back(std::move(debug_state));
            }
          }
        }
        if (TabListInterface* tab_list = TabListInterface::From(browser)) {
          tabs::TabInterface* active_tab = tab_list->GetActiveTab();
          for (int i = 0; i < tab_list->GetTabCount(); ++i) {
            tabs::TabInterface* tab = tab_list->GetTab(i);
            content::WebContents* tab_wc = tab ? tab->GetContents() : nullptr;
            if (!tab_wc || !seen_web_contents.insert(tab_wc).second) {
              continue;
            }
            if (auto debug_state = BuildWebContentsDebugState(
                    tab_wc, /*is_side_panel=*/false,
                    /*is_active_tab_in_window=*/(tab == active_tab))) {
              results.push_back(std::move(debug_state));
            }
          }
        }
        return true;
      });

  std::move(callback).Run(std::move(results));
}

contextual_tasks_internals::mojom::WebContentsDebugStatePtr
ContextualTasksInternalsPageHandler::BuildWebContentsDebugState(
    content::WebContents* web_contents,
    bool is_side_panel,
    bool is_active_tab_in_window) const {
  if (!web_contents) {
    return nullptr;
  }

  const auto* user_data =
      contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
          web_contents);
  const auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents);
  const contextual_search::ContextualSearchSessionHandle* session_handle =
      user_data ? user_data->GetExistingContextualSessionHandle() : nullptr;
  if (!session_handle && helper) {
    session_handle = helper->session_handle();
  }

  if (!user_data && !session_handle) {
    return nullptr;
  }

  auto state = contextual_tasks_internals::mojom::WebContentsDebugState::New();
  state->is_side_panel = is_side_panel;
  state->title = base::UTF16ToUTF8(web_contents->GetTitle());
  state->url = web_contents->GetLastCommittedURL();
  SessionID session_tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  state->session_tab_id = session_tab_id.is_valid()
                              ? std::make_optional(session_tab_id.id())
                              : std::nullopt;
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  state->tab_handle_id =
      tab_interface ? std::make_optional(tab_interface->GetHandle().raw_value())
                    : std::nullopt;
  state->is_active_tab_in_window = is_active_tab_in_window;

  contextual_tasks::ContextualTasksService* tasks_service =
      profile_ ? contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
                     profile_)
               : nullptr;
  state->user_data_state =
      BuildUserDataDebugState(user_data, helper, tasks_service, session_tab_id);
  return state;
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
