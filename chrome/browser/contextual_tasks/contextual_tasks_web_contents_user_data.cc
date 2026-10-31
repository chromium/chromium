// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/containers/adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/ai_mode_context_library_converter.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/site_exclusion_detail.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/desktop_query_contextualizer_delegate.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_api_frame_id_map.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif

namespace contextual_tasks {

namespace {

constexpr size_t kMaxSearchMessageBytes = 1024 * 1024;
constexpr size_t kMaxPendingSearchMessages = 100;

struct IdentityState {
  bool is_signed_in = false;
  bool browser_identity_matches_aim_identity = false;
};

IdentityState GetIdentityState(content::WebContents* web_contents) {
  if (!web_contents) {
    return {};
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* ui_service = profile
                         ? contextual_tasks::ContextualTasksUiServiceFactory::
                               GetForBrowserContext(profile)
                         : nullptr;
  GURL url = web_contents->GetLastCommittedURL();
  IdentityState state;
  if (ui_service) {
    state.is_signed_in = ui_service->IsSignedInToBrowserWithValidCredentials();
    state.browser_identity_matches_aim_identity =
        state.is_signed_in && ui_service->IsUrlForPrimaryAccount(url);
  } else if (profile && omnibox::kComposeboxDriveIdentityFallback.Get()) {
    if (auto* identity_manager =
            IdentityManagerFactory::GetForProfile(profile)) {
      if (contextual_tasks::IsSignedInToBrowserWithValidCredentials(
              identity_manager)) {
        state.is_signed_in = true;
        state.browser_identity_matches_aim_identity =
            contextual_tasks::IsUrlForPrimaryAccount(identity_manager, url);
      }
    }
  }
  return state;
}

std::optional<int32_t> FindOpenTabHandleForSessionTabId(
    int32_t session_tab_id,
    BrowserWindowInterface* browser_window_interface) {
  if (!browser_window_interface) {
    return std::nullopt;
  }
  auto* tab_list = TabListInterface::From(browser_window_interface);
  if (!tab_list) {
    return std::nullopt;
  }
  for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
    if (!tab || !tab->GetContents()) {
      continue;
    }
    const SessionID open_tab_session_id =
        sessions::SessionTabHelper::IdForTab(tab->GetContents());
    if (open_tab_session_id.is_valid() &&
        open_tab_session_id.id() == session_tab_id) {
      return tab->GetHandle().raw_value();
    }
  }
  return std::nullopt;
}

// An open tab that a restored tab can be resolved to.
struct OpenTabInfo {
  int32_t session_id;
  GURL url;
  std::string title;
};

// Returns the open tabs in `browser_window`.
std::vector<OpenTabInfo> GetOpenTabs(BrowserWindowInterface* browser_window) {
  std::vector<OpenTabInfo> open_tabs;
  TabListInterface* tab_list =
      browser_window ? TabListInterface::From(browser_window) : nullptr;
  if (!tab_list) {
    return open_tabs;
  }
  for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
    content::WebContents* contents = tab ? tab->GetContents() : nullptr;
    if (!contents) {
      continue;
    }
    const SessionID session_id = sessions::SessionTabHelper::IdForTab(contents);
    if (session_id.is_valid()) {
      open_tabs.push_back({session_id.id(), contents->GetLastCommittedURL(),
                           base::UTF16ToUTF8(contents->GetTitle())});
    }
  }
  return open_tabs;
}

// Returns whether tabs can be deselected after they were submitted. Matches
// `ContextualSearchSessionHandle`, which only reports deselected submitted tabs
// to the server in `removed_contexts` under these flags.
bool IsTabDeselectionEnabled() {
  return omnibox::IsTabDeselectionInComposeboxEnabled() ||
         IsContextualTasksRearchitectureEnabled() ||
         IsContextualTasksSidePanelRearchitectureEnabled();
}

#if !BUILDFLAG(IS_ANDROID)
void RemoveAttachedTabUnderline(
    std::optional<int32_t> session_tab_id,
    const ContextualTasksWebContentsUserData& user_data,
    BrowserWindowInterface* browser_window_interface) {
  if (!session_tab_id.has_value() || !browser_window_interface) {
    return;
  }
  if (std::optional<int32_t> open_tab_handle_id =
          FindOpenTabHandleForSessionTabId(*session_tab_id,
                                           browser_window_interface)) {
    contextual_tasks::RemoveTabUnderline(*open_tab_handle_id,
                                         browser_window_interface);
    return;
  }
  if (std::optional<int32_t> tab_handle_id =
          user_data.GetTabHandleForSessionTabId(*session_tab_id)) {
    contextual_tasks::RemoveTabUnderline(*tab_handle_id,
                                         browser_window_interface);
  }
}

lens::LensOverlayVisualSearchInteractionData CreateCropInteractionData(
    const lens::mojom::CenterRotatedBoxPtr& region,
    const SkBitmap& screenshot) {
  lens::LensOverlayVisualSearchInteractionData vsint;
  vsint.set_interaction_type(
      lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  auto* mutable_zoomed_crop = vsint.mutable_zoomed_crop();
  mutable_zoomed_crop->set_parent_height(screenshot.height());
  mutable_zoomed_crop->set_parent_width(screenshot.width());
  mutable_zoomed_crop->set_zoom(1.0);
  auto* crop = mutable_zoomed_crop->mutable_crop();
  crop->set_coordinate_type(lens::CoordinateType::NORMALIZED);
  if (region->coordinate_type ==
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized) {
    crop->set_center_x(region->box.x());
    crop->set_center_y(region->box.y());
    crop->set_width(region->box.width());
    crop->set_height(region->box.height());
  } else {
    crop->set_center_x(region->box.x() / screenshot.width());
    crop->set_center_y(region->box.y() / screenshot.height());
    crop->set_width(region->box.width() / screenshot.width());
    crop->set_height(region->box.height() / screenshot.height());
  }
  vsint.mutable_log_data()->mutable_filter_data()->set_filter_type(
      lens::AUTO_FILTER);
  vsint.mutable_log_data()->mutable_user_selection_data()->set_selection_type(
      lens::MULTIMODAL_SEARCH);
  vsint.mutable_log_data()->set_is_parent_query(true);
  vsint.mutable_log_data()->set_client_platform(
      lens::CLIENT_PLATFORM_LENS_OVERLAY);
  return vsint;
}
#endif

}  // namespace

ContextualTasksWebContentsUserData::ExtensionFrameInfo::ExtensionFrameInfo() =
    default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo::ExtensionFrameInfo(
    const ExtensionFrameInfo&) = default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo&
ContextualTasksWebContentsUserData::ExtensionFrameInfo::operator=(
    const ExtensionFrameInfo&) = default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo::ExtensionFrameInfo(
    ExtensionFrameInfo&&) = default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo&
ContextualTasksWebContentsUserData::ExtensionFrameInfo::operator=(
    ExtensionFrameInfo&&) = default;
ContextualTasksWebContentsUserData::ExtensionFrameInfo::~ExtensionFrameInfo() =
    default;

struct ContextualTasksWebContentsUserData::PageSearchState
    : public content::PageUserData<
          ContextualTasksWebContentsUserData::PageSearchState> {
  explicit PageSearchState(content::Page& page)
      : content::PageUserData<PageSearchState>(page) {}
  ~PageSearchState() override { RecordTeardownMetricsIfNeeded(); }

  void RecordTeardownMetricsIfNeeded() {
    if (handshake_start_time.has_value() && !handshake_status_recorded) {
      handshake_status_recorded = true;
      RecordExtensionHandshakeStatus(ExtensionHandshakeStatus::kFrameDetached);
    }
    if ((handshake_start_time.has_value() || !pending_search_messages.empty() ||
         dropped_overflow_messages > 0) &&
        !queued_messages_dropped_recorded) {
      queued_messages_dropped_recorded = true;
      RecordExtensionQueuedSearchMessagesDropped(
          pending_search_messages.size() + dropped_overflow_messages);
    }
    handshake_start_time.reset();
    pending_search_messages.clear();
    dropped_overflow_messages = 0;
  }

  std::string connected_document_id;
  ServiceWorkerPostMessageCallback service_worker_post_message_cb;
  std::optional<base::TimeTicks> handshake_start_time;
  bool handshake_status_recorded = false;
  bool queued_messages_dropped_recorded = false;
  size_t dropped_overflow_messages = 0;
  bool is_handshake_complete = false;
  std::optional<base::TimeTicks> handshake_complete_time;
  bool has_recorded_first_thread_context_library_update = false;
  std::vector<std::vector<uint8_t>> pending_search_messages;
  bool is_lens_crop_mounted = false;
  std::string last_lens_crop_data_uri;
  bool context_library_is_active = false;
  bool is_zero_state = false;
  ContextState last_sent_context_state = ContextState::kNone;
  std::string last_sent_suggest_inputs_serialized;

  PAGE_USER_DATA_KEY_DECL();
};

PAGE_USER_DATA_KEY_IMPL(ContextualTasksWebContentsUserData::PageSearchState);

ContextualTasksWebContentsUserData::ContextualTasksWebContentsUserData(
    content::WebContents* contents)
    : content::WebContentsUserData<ContextualTasksWebContentsUserData>(
          *contents),
      content::WebContentsObserver(contents),
      // The callbacks are only run by `query_contextualizer_`, which is owned
      // by `this`, so `base::Unretained` is safe.
      query_contextualizer_delegate_(std::make_unique<
                                     DesktopQueryContextualizerDelegate>(
          base::BindRepeating(&ContextualTasksWebContentsUserData::
                                  GetOrCreateContextualSessionHandle,
                              base::Unretained(this)),
          base::BindRepeating(
              []() -> std::optional<lens::ImageEncodingOptions> {
                return CreateImageEncodingOptions();
              }),
          // The delegate only uses its `ContextualTasksContextService` for
          // smart tab selection, which is disabled here. Recontextualization
          // uses the `ContextualTasksService` passed to
          // `query_contextualizer_`.
          /*service=*/nullptr,
          base::BindRepeating(
              &ContextualTasksWebContentsUserData::GetBrowserWindowInterface,
              base::Unretained(this)))),
      query_contextualizer_(std::make_unique<QueryContextualizer>(
          ContextualTasksServiceFactory::GetForProfile(
              Profile::FromBrowserContext(contents->GetBrowserContext())),
          query_contextualizer_delegate_.get())) {
  if (auto* service = ContextualTasksServiceFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()))) {
    contextual_tasks_service_observation_.Observe(service);
  }
  auto* helper =
      ContextualSearchWebContentsHelper::GetOrCreateForWebContents(contents);
  // Safe to use base::Unretained(this) because `session_changed_subscription_`
  // is owned by `this` and unsubscribes on destruction.
  session_changed_subscription_ = helper->RegisterSessionChangedCallback(
      base::BindRepeating(&ContextualTasksWebContentsUserData::OnSessionChanged,
                          base::Unretained(this)));
  MaybeRefreshTabContextSubscription(helper->session_handle());
}

ContextualTasksWebContentsUserData::~ContextualTasksWebContentsUserData() {
  if (observed_controller_) {
    observed_controller_->RemoveObserver(this);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualTasksWebContentsUserData);

void ContextualTasksWebContentsUserData::set_input_state_model(
    std::unique_ptr<contextual_search::InputStateModel> input_state_model) {
  if (!input_state_model) {
    return;
  }
  if (auto* handle = input_state_model->session_handle()) {
    last_active_model_ = input_state_model->AsWeakPtr();
    input_state_models_[handle->session_id()] = std::move(input_state_model);
    SubscribeToInputStateModel(last_active_model_);
    MaybeRefreshTabContextSubscription(handle);
  }
}

// static
void ContextualTasksWebContentsUserData::UpdateInputStateModelIdentity(
    content::WebContents* web_contents,
    contextual_search::InputStateModel* input_state_model) {
  if (!web_contents || !input_state_model) {
    return;
  }
  IdentityState identity_state = GetIdentityState(web_contents);
  input_state_model->SetIdentityState(
      identity_state.is_signed_in,
      identity_state.browser_identity_matches_aim_identity);
}

base::WeakPtr<contextual_search::InputStateModel>
ContextualTasksWebContentsUserData::GetOrCreateInputStateModel(
    contextual_search::ContextualSearchSessionHandle& session_handle) {
  // Garbage collect models whose session handles have been destroyed
  base::EraseIf(input_state_models_, [](const auto& pair) {
    return !pair.second->session_handle();
  });

  content::WebContents* web_contents = &GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* service = AimEligibilityServiceFactory::GetForProfile(profile);
  const omnibox::SearchboxConfig* config =
      service ? service->GetSearchboxConfig() : nullptr;

  IdentityState identity_state = GetIdentityState(web_contents);

  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents);
  contextual_search::ContextualSearchSessionHandle* web_contents_session =
      helper ? (task_id_.has_value() ? helper->GetSessionForTask(*task_id_)
                                     : helper->session_handle())
             : nullptr;
  const bool is_web_contents_session =
      (&session_handle == web_contents_session);

  auto it = input_state_models_.find(session_handle.session_id());
  if (it != input_state_models_.end()) {
    it->second->SetIdentityState(
        identity_state.is_signed_in,
        identity_state.browser_identity_matches_aim_identity);
    if (config) {
      it->second->UpdateConfig(*config);
    }
    if (is_web_contents_session || !subscribed_model_) {
      last_active_model_ = it->second->AsWeakPtr();
    }
    if (is_web_contents_session) {
      SubscribeToInputStateModel(it->second->AsWeakPtr());
      MaybeRefreshTabContextSubscription(&session_handle);
    }
    return it->second->AsWeakPtr();
  }

  GURL url = web_contents->GetLastCommittedURL();
  bool is_off_the_record = profile->IsOffTheRecord();

  auto model = std::make_unique<contextual_search::InputStateModel>(
      session_handle, config ? *config : omnibox::SearchboxConfig(), url,
      is_off_the_record, identity_state.is_signed_in,
      identity_state.browser_identity_matches_aim_identity);
  if (profile) {
    model->SetPrefService(profile->GetPrefs());
  }

  auto model_weak = model->AsWeakPtr();
  if (is_web_contents_session || !subscribed_model_) {
    last_active_model_ = model_weak;
  }
  input_state_models_[session_handle.session_id()] = std::move(model);
  if (is_web_contents_session) {
    SubscribeToInputStateModel(model_weak);
    MaybeRefreshTabContextSubscription(&session_handle);
  }
  return model_weak;
}

base::WeakPtr<contextual_search::InputStateModel>
ContextualTasksWebContentsUserData::GetOrCreateInputStateModel() {
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return nullptr;
  }
  auto model = GetOrCreateInputStateModel(*session_handle);
  SubscribeToInputStateModel(model);
  return model;
}

void ContextualTasksWebContentsUserData::SubscribeToInputStateModel(
    base::WeakPtr<contextual_search::InputStateModel> model) {
  if (subscribed_model_.get() == model.get()) {
    return;
  }
  subscribed_model_ = model;
  auto& page_state = GetPrimaryPageSearchState();
  page_state.is_lens_crop_mounted = false;
  page_state.last_lens_crop_data_uri.clear();
  if (subscribed_model_) {
    // Safe to use base::Unretained(this) because `input_state_subscription_` is
    // owned by `this` and unsubscribes on destruction.
    input_state_subscription_ =
        subscribed_model_->Subscribe(base::BindRepeating(
            &ContextualTasksWebContentsUserData::OnInputStateChanged,
            base::Unretained(this)));
  } else {
    input_state_subscription_ = {};
  }
}

void ContextualTasksWebContentsUserData::SetTaskId(
    std::optional<base::Uuid> task_id) {
  if (task_id.has_value() && task_id_.has_value() && task_id != task_id_) {
    OnTaskChanged();
  }
  task_id_ = task_id;
  if (!task_id_) {
    return;
  }
  // Moves to `task_id`'s session. An untasked session and its tabs are adopted
  // by `task_id` (see `GetSessionForTask()`), so do not reset its tabs. The new
  // session will adopt these unassigned tabs.
  OnSessionChanged(GetOrCreateContextualSessionHandle());
  GetOrCreateInputStateModel();
}

void ContextualTasksWebContentsUserData::OnTaskChanged() {
  ClearInputPlateState();
}

void ContextualTasksWebContentsUserData::RegisterExtensionFrame(
    const void* handler_id) {
  for (auto& frame : extension_frames_) {
    if (frame.handler_id == handler_id) {
      return;
    }
  }
  ExtensionFrameInfo info;
  info.handler_id = handler_id;
  info.is_page_bound = false;
  extension_frames_.push_back(std::move(info));
}

void ContextualTasksWebContentsUserData::UpdateExtensionFrameBound(
    const void* handler_id,
    bool is_page_bound,
    base::RepeatingCallback<void(const lens::ClientToSearchMessage&)>
        post_search_message_cb,
    base::RepeatingClosure on_handshake_complete_cb,
    base::RepeatingCallback<void(const GURL&)> on_lens_crop_updated_cb) {
  bool had_bound_frame = HasBoundExtensionFrame();
  for (auto& frame : extension_frames_) {
    if (frame.handler_id == handler_id) {
      frame.is_page_bound = is_page_bound;
      if (!is_page_bound) {
        frame.is_handshake_complete = false;
      }
      frame.post_search_message_cb = std::move(post_search_message_cb);
      frame.on_handshake_complete_cb = std::move(on_handshake_complete_cb);
      frame.on_lens_crop_updated_cb = std::move(on_lens_crop_updated_cb);
      if (is_page_bound && !had_bound_frame &&
          !IsServiceWorkerPortConnected()) {
        auto& page_state = GetPrimaryPageSearchState();
        page_state.is_lens_crop_mounted = false;
        page_state.last_lens_crop_data_uri.clear();
        page_state.last_sent_context_state = ContextState::kNone;
        page_state.last_sent_suggest_inputs_serialized.clear();
      }
      return;
    }
  }
}

void ContextualTasksWebContentsUserData::UnregisterExtensionFrame(
    const void* handler_id) {
  if (last_active_search_handler_id_ == handler_id) {
    last_active_search_handler_id_ = nullptr;
  }
  bool was_bound = false;
  std::erase_if(extension_frames_, [&](const ExtensionFrameInfo& frame) {
    if (frame.handler_id == handler_id) {
      was_bound = frame.is_page_bound;
      return true;
    }
    return false;
  });

  if (was_bound) {
    if (const ExtensionFrameInfo* target_frame = FindTargetExtensionFrame();
        target_frame && target_frame->is_handshake_complete &&
        !GetWebContents().IsBeingDestroyed() &&
        GetWebContents().GetPrimaryMainFrame() &&
        !IsServiceWorkerPortConnected()) {
      // An unmounting extension iframe (such as `tab_picker.html` when the `+`
      // menu closes) may have been the last frame to postMessage to Search,
      // leaving Search's `ChromePostMessageService.activeChromeWindow` pointing
      // to the detached window. Re-sending the current context state through a
      // surviving handshake-complete frame (such as `context_library.html`)
      // forces Search to switch `activeChromeWindow` to the surviving frame.
      last_active_search_handler_id_ = target_frame->handler_id;
      UpdateContextState(/*force_send=*/true);
    }
  }
}

bool ContextualTasksWebContentsUserData::HasBoundExtensionFrame() const {
  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound) {
      return true;
    }
  }

  return false;
}

void ContextualTasksWebContentsUserData::OnDocumentConnected(
    content::RenderFrameHost* main_rfh,
    ServiceWorkerPostMessageCallback post_message_cb) {
  if (!main_rfh) {
    return;
  }
  auto& page_state = *PageSearchState::GetOrCreateForPage(main_rfh->GetPage());
  if (page_state.handshake_start_time.has_value()) {
    page_state.RecordTeardownMetricsIfNeeded();
  }
  std::string doc_id;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  doc_id =
      extensions::ExtensionApiFrameIdMap::GetDocumentId(main_rfh).ToString();
#endif
  if (page_state.connected_document_id != doc_id) {
    page_state.last_lens_crop_data_uri.clear();
  }
  page_state.pending_search_messages.clear();
  page_state.is_lens_crop_mounted = false;
  page_state.context_library_is_active = false;
  page_state.is_zero_state = false;
  page_state.last_sent_context_state = ContextState::kNone;
  page_state.last_sent_suggest_inputs_serialized.clear();
  page_state.connected_document_id = doc_id;
  page_state.service_worker_post_message_cb = std::move(post_message_cb);
  page_state.handshake_start_time = base::TimeTicks::Now();
  page_state.handshake_status_recorded = false;
  page_state.queued_messages_dropped_recorded = false;
  page_state.dropped_overflow_messages = 0;
  page_state.is_handshake_complete = false;
  page_state.handshake_complete_time.reset();
  page_state.has_recorded_first_thread_context_library_update = false;
  GetOrCreateContextualSessionHandle();
}

bool ContextualTasksWebContentsUserData::IsServiceWorkerPortConnected() const {
  const auto* page_state = GetPrimaryPageSearchState();
  if (!page_state || page_state->connected_document_id.empty()) {
    return false;
  }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  content::RenderFrameHost* primary_rfh =
      const_cast<content::WebContents&>(GetWebContents()).GetPrimaryMainFrame();
  if (!primary_rfh) {
    return false;
  }
  return extensions::ExtensionApiFrameIdMap::GetDocumentId(primary_rfh)
             .ToString() == page_state->connected_document_id;
#else
  return true;
#endif
}

bool ContextualTasksWebContentsUserData::IsHandshakeCompleteForTesting() const {
  const auto* page_state = GetPrimaryPageSearchState();
  return page_state && page_state->is_handshake_complete;
}

const std::string&
ContextualTasksWebContentsUserData::GetConnectedDocumentIdForTesting() const {
  const auto* page_state = GetPrimaryPageSearchState();
  return page_state ? page_state->connected_document_id : base::EmptyString();
}

bool ContextualTasksWebContentsUserData::OnSearchMessageReceived(
    base::span<const uint8_t> message,
    const void* source_handler_id) {
  if (message.size() > kMaxSearchMessageBytes) {
    return false;
  }

  lens::SearchToClientMessage search_to_client_message;
  if (!search_to_client_message.ParseFromArray(message.data(),
                                               message.size())) {
    return false;
  }

  if (search_to_client_message.event_message_case() !=
          lens::SearchToClientMessage::EVENT_MESSAGE_NOT_SET &&
      source_handler_id) {
    last_active_search_handler_id_ = source_handler_id;
  }

  switch (search_to_client_message.event_message_case()) {
    case lens::SearchToClientMessage::kHandshakeResponse: {
      int32_t raw_auth_index =
          search_to_client_message.handshake_response().auth_user_index();
      size_t auth_user_index = static_cast<size_t>(std::max(0, raw_auth_index));
      if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
        session_handle->set_auth_user_index(auth_user_index);
      }
      OnHandshakeComplete(source_handler_id);
      return true;
    }
    case lens::SearchToClientMessage::kOnSubmitQueryRequest:
      HandleOnSubmitQueryRequest();
      return true;
    case lens::SearchToClientMessage::kOpenLinkInSidePanelMode:
      HandleOpenLinkInSidePanelMode(
          search_to_client_message.open_link_in_side_panel_mode().url());
      return true;
    case lens::SearchToClientMessage::kUpdateThreadContextLibrary:
      HandleThreadContextLibraryUpdateFromAim(
          search_to_client_message.update_thread_context_library());
      return true;
    case lens::SearchToClientMessage::kOnPageChanged:
      OnPageChanged(search_to_client_message.on_page_changed().is_zero_state());
      return true;
    case lens::SearchToClientMessage::EVENT_MESSAGE_NOT_SET:
      return false;
  }
  return false;
}

void ContextualTasksWebContentsUserData::OnHandshakeComplete(
    const void* source_handler_id) {
  if (source_handler_id) {
    last_active_search_handler_id_ = source_handler_id;
    for (auto& frame : extension_frames_) {
      if (frame.handler_id == source_handler_id && frame.is_page_bound) {
        frame.is_handshake_complete = true;
        if (frame.on_handshake_complete_cb) {
          frame.on_handshake_complete_cb.Run();
        }
        break;
      }
    }
  } else {
    for (auto& frame : extension_frames_) {
      if (frame.is_page_bound) {
        frame.is_handshake_complete = true;
        if (frame.on_handshake_complete_cb) {
          frame.on_handshake_complete_cb.Run();
        }
      }
    }
  }
  auto& page_state = GetPrimaryPageSearchState();
  if (!source_handler_id && page_state.handshake_start_time.has_value() &&
      !page_state.handshake_status_recorded) {
    page_state.handshake_status_recorded = true;
    RecordExtensionHandshakeLatency(base::TimeTicks::Now() -
                                    *page_state.handshake_start_time);
    RecordExtensionHandshakeStatus(ExtensionHandshakeStatus::kSuccess);
  }
  const bool is_first_page_handshake = !page_state.is_handshake_complete;
  if (is_first_page_handshake) {
    page_state.handshake_complete_time = base::TimeTicks::Now();
    RecordTimeToHandshakeComplete();
  }
  page_state.is_handshake_complete = true;

  const bool had_queued_messages = !page_state.pending_search_messages.empty();
  std::vector<std::vector<uint8_t>> queued =
      std::exchange(page_state.pending_search_messages, {});
  for (const auto& msg_bytes : queued) {
    DispatchSerializedSearchMessage(msg_bytes);
  }

  if (!had_queued_messages && !IsServiceWorkerPortConnected()) {
    if (is_first_page_handshake) {
      page_state.context_library_is_active = false;
    }
    page_state.last_sent_context_state = ContextState::kNone;
    page_state.last_sent_suggest_inputs_serialized.clear();
  }

  if (HasContextLibraryTabs() && !page_state.context_library_is_active) {
    SendMountContextLibrary();
  }
  if (auto model = GetOrCreateInputStateModel()) {
    if (model->lens_crop().has_value() && !page_state.is_lens_crop_mounted) {
      page_state.is_lens_crop_mounted = true;
      SendInjectChromeInput(InjectedInputType::kLensChip, /*is_active=*/true);
    }
  }
  UpdateContextState();
  UpdateSuggestInputs();
}

void ContextualTasksWebContentsUserData::PostSearchMessage(
    const lens::ClientToSearchMessage& message) {
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> bytes(size);
  message.SerializeToArray(bytes.data(), size);

  auto& page_state = GetPrimaryPageSearchState();
  if (IsServiceWorkerPortConnected()) {
    if (!page_state.is_handshake_complete) {
      if (page_state.pending_search_messages.size() <
          kMaxPendingSearchMessages) {
        page_state.pending_search_messages.push_back(std::move(bytes));
      } else {
        ++page_state.dropped_overflow_messages;
      }
      return;
    }
    DispatchSerializedSearchMessage(bytes);
    return;
  }

  // Fallback when Service Worker Port is not connected: route via the active
  // bound extension iframe if present.
  if (const ExtensionFrameInfo* target_frame = FindTargetExtensionFrame()) {
    target_frame->post_search_message_cb.Run(message);
    if (search_message_dispatcher_for_testing_) {
      search_message_dispatcher_for_testing_.Run(message);
    }
    return;
  }

  // Zero bound extension frames and no Service Worker Port connected yet:
  // queue until handshake completes (or dispatch to testing callback if
  // handshake already completed).
  if (!page_state.is_handshake_complete) {
    if (page_state.pending_search_messages.size() < kMaxPendingSearchMessages) {
      page_state.pending_search_messages.push_back(std::move(bytes));
    } else {
      ++page_state.dropped_overflow_messages;
    }
    return;
  }
  DispatchSerializedSearchMessage(bytes);
}

void ContextualTasksWebContentsUserData::SendInjectChromeInput(
    InjectedInputType type,
    bool is_active) {
  lens::ClientToSearchMessage inject_msg;
  auto* inject_input = inject_msg.mutable_inject_chrome_input();
  switch (type) {
    case InjectedInputType::kContextLibrary:
      inject_input->set_input_type(
          lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
      break;
    case InjectedInputType::kLensChip:
      inject_input->set_input_type(
          lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
      break;
  }
  inject_input->set_is_active(is_active);
  PostSearchMessage(inject_msg);
}

void ContextualTasksWebContentsUserData::SendMountContextLibrary() {
  GetPrimaryPageSearchState().context_library_is_active = true;
  SendInjectChromeInput(InjectedInputType::kContextLibrary,
                        /*is_active=*/true);
}

void ContextualTasksWebContentsUserData::UpdateContextLibraryInputState() {
  if (GetSelectedTabs().empty()) {
    pending_context_uploads_.clear();
    failed_context_uploads_.clear();
  }
  // Submitted and restored tabs are still shown by the context library, so
  // only unmount it once the session has no tabs left to show.
  const bool has_tabs = HasContextLibraryTabs();
  auto& page_state = GetPrimaryPageSearchState();
  if (has_tabs != page_state.context_library_is_active) {
    page_state.context_library_is_active = has_tabs;
    SendInjectChromeInput(InjectedInputType::kContextLibrary,
                          /*is_active=*/has_tabs);
  }
  UpdateContextState();
  UpdateSuggestInputs();
}

bool ContextualTasksWebContentsUserData::HasContextLibraryTabs() {
  if (!subscribed_session_handle_) {
    return false;
  }
  // Attached tabs include both unsubmitted and submitted (persisted) tabs.
  const auto& tab_context = subscribed_session_handle_->GetTabContextState();
  return !tab_context.attached.empty() || !tab_context.restored.empty();
}

void ContextualTasksWebContentsUserData::OnTabContextUploadStarted(
    const base::UnguessableToken& context_token) {
  pending_context_uploads_.insert(context_token);
  failed_context_uploads_.erase(context_token);
}

void ContextualTasksWebContentsUserData::OnTabContextRemoved(
    const base::UnguessableToken& context_token) {
  pending_context_uploads_.erase(context_token);
  failed_context_uploads_.erase(context_token);
}

void ContextualTasksWebContentsUserData::OnContextUploadStatusChanged(
    const base::UnguessableToken& context_token,
    lens::MimeType mime_type,
    contextual_search::ContextUploadStatus context_upload_status,
    const std::optional<contextual_search::ContextUploadErrorType>&
        error_type) {
  if (contextual_search::IsTerminalContextStatus(context_upload_status)) {
    pending_context_uploads_.erase(context_token);
    if (context_upload_status ==
        contextual_search::ContextUploadStatus::kUploadSuccessful) {
      failed_context_uploads_.erase(context_token);
    } else {
      failed_context_uploads_.insert(context_token);
    }
  } else {
    pending_context_uploads_.insert(context_token);
    failed_context_uploads_.erase(context_token);
  }
  UpdateContextState();
  UpdateSuggestInputs();
}

void ContextualTasksWebContentsUserData::OnTaskDisassociatedFromTab(
    const base::Uuid& task_id,
    SessionID tab_id) {
  if (GetEffectiveTaskId() != task_id) {
    return;
  }
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return;
  }
  base::UnguessableToken token = session_handle->GetTokenForTab(tab_id);
  if (token.is_empty()) {
    auto selected_tabs = GetSelectedTabs();
    if (auto it = std::ranges::find_if(
            selected_tabs,
            [&](const contextual_search::TabInfo& tab) {
              return tab.tab_id == tab_id.id();
            });
        it != selected_tabs.end()) {
      token = it->context_token;
    }
  }
  if (!token.is_empty()) {
    DeleteContext(token);
  }
}

void ContextualTasksWebContentsUserData::OnWillBeDestroyed() {
  contextual_tasks_service_observation_.Reset();
}

void ContextualTasksWebContentsUserData::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (old_host && new_host && new_host->IsInPrimaryMainFrame()) {
    if (auto* old_state = PageSearchState::GetForPage(old_host->GetPage())) {
      old_state->RecordTeardownMetricsIfNeeded();
    }
  }
}

void ContextualTasksWebContentsUserData::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host && render_frame_host->IsInPrimaryMainFrame()) {
    if (auto* page_state =
            PageSearchState::GetForPage(render_frame_host->GetPage())) {
      page_state->RecordTeardownMetricsIfNeeded();
    }
  }
}

void ContextualTasksWebContentsUserData::HandleOnSubmitQueryRequest() {
  content::WebContents* web_contents = &GetWebContents();
  // Require a recent user interaction on the page before attaching context.
  if (!web_contents->HasRecentInteraction() || submit_pending_) {
    return;
  }

  // Prevent replay by ensuring each submission consumes a new interaction.
  base::TimeTicks last_interaction =
      web_contents->GetLastInteractionTimeTicks();
  if (last_interaction <= last_handled_submit_interaction_time_) {
    return;
  }
  last_handled_submit_interaction_time_ = last_interaction;

  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    lens::ClientToSearchMessage response_message;
    response_message.mutable_on_submit_query_response();
    PostSearchMessage(response_message);
    return;
  }

  // Read the Lens state now, since it may change while contextualizing.
  std::optional<base::UnguessableToken> overlay_token = GetLensOverlayToken();
  std::optional<lens::AddedContext> lens_added_context = GetLensAddedContext();

  QueryContextualizer::ContextualizeParams params;
  // Without a task (e.g. full-tab AI Mode in zero state),
  // `query_contextualizer_` only uploads the delayed tabs (no
  // recontextualization).
  params.task_id = GetEffectiveTaskId();
  params.tabs_to_recontextualize =
      GetTabsToRecontextualize(*session_handle, overlay_token);
  params.auto_suggested_chip_tabs = GetPendingDelayedTabs(*session_handle);
  // TODO(crbug.com/572825497): Support smart tab sharing, like
  // `ContextualTasksComposeboxHandler`.
  params.enable_smart_tab_selection = false;
  // The contextualizer's session handle is ignored: it is null when there was
  // nothing to contextualize, so the callback gets the session handle itself.
  params.complete_callback = base::IgnoreArgs<
      base::WeakPtr<contextual_search::ContextualSearchSessionHandle>>(
      base::BindOnce(
          &ContextualTasksWebContentsUserData::OnSubmitQueryContextualized,
          weak_ptr_factory_.GetWeakPtr(), std::move(lens_added_context),
          overlay_token));
  submit_pending_ = true;
  query_contextualizer_->Contextualize(std::move(params));
}

std::vector<int32_t>
ContextualTasksWebContentsUserData::GetTabsToRecontextualize(
    contextual_search::ContextualSearchSessionHandle& session_handle,
    const std::optional<base::UnguessableToken>& overlay_token) {
  // Recontextualize the active tab (if it is an attachment of the task),
  // unless the query has a visual selection or the tab was deselected.
  auto* browser_window_interface = GetBrowserWindowInterface();
  auto* tab_list = browser_window_interface
                       ? TabListInterface::From(browser_window_interface)
                       : nullptr;
  tabs::TabInterface* active_tab =
      tab_list ? tab_list->GetActiveTab() : nullptr;
  if (!active_tab || overlay_token.has_value()) {
    return {};
  }
  SessionID active_tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  const bool tracked_in_session =
      !session_handle.GetTokenForTab(active_tab_session_id).is_empty() ||
      session_handle.GetTabContextState().ContainsTab(active_tab_session_id);
  if (!tracked_in_session) {
    return {};
  }
  if (IsTabDeselectionEnabled() &&
      session_handle.IsTabDeselected(
          active_tab_session_id,
          active_tab->GetContents()->GetLastCommittedURL(),
          base::UTF16ToUTF8(active_tab->GetTitle()))) {
    return {};
  }
  return {active_tab->GetHandle().raw_value()};
}

std::vector<int32_t> ContextualTasksWebContentsUserData::GetPendingDelayedTabs(
    contextual_search::ContextualSearchSessionHandle& session_handle) {
  std::vector<int32_t> tabs;
  for (SessionID delayed_tab_id : session_handle.GetPendingDelayedTabIds()) {
    // Delayed tabs are mapped when added in `AddTabContext()`.
    if (std::optional<int32_t> tab_handle_id =
            GetTabHandleForSessionTabId(delayed_tab_id.id())) {
      tabs.push_back(*tab_handle_id);
    }
  }
  return tabs;
}

void ContextualTasksWebContentsUserData::OnSubmitQueryContextualized(
    std::optional<lens::AddedContext> lens_added_context,
    std::optional<base::UnguessableToken> overlay_token) {
  if (!submit_pending_) {
    return;
  }
  submit_pending_ = false;

  lens::ClientToSearchMessage response_message;
  auto* on_submit_response =
      response_message.mutable_on_submit_query_response();
  if (lens_added_context) {
    *on_submit_response->add_added_contexts() = std::move(*lens_added_context);
  }
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (session_handle) {
    AppendTabContextsToOnSubmitQueryResponse(&response_message, session_handle,
                                             overlay_token);
  }
  PostSearchMessage(response_message);

  if (session_handle) {
    // The page owns the query text, so no query length metrics are recorded
    // here. This moves the uploaded tabs into the session's persisted tabs so
    // they are tracked across turns and are not re-added on the next
    // submission.
    session_handle->MarkQuerySubmitted(/*file_tokens=*/{},
                                       /*query_text_length=*/std::nullopt);
  }

  // Submission moved the uploaded tabs into the session's persisted tabs, so
  // the context library chip state may have changed.
  UpdateContextLibraryInputState();

  DoSubmitQueryCleanup();
}

void ContextualTasksWebContentsUserData::OnPageChanged(bool is_zero_state) {
  if (is_zero_state) {
    zero_state_callbacks_.Notify();
  }
  if (!std::exchange(preserve_prestaged_context_, false) || !is_zero_state) {
    ClearInputPlateState();
  }
  auto& page_state = GetPrimaryPageSearchState();
  page_state.is_zero_state = is_zero_state;
  if (page_state.is_handshake_complete) {
    page_state.handshake_complete_time = base::TimeTicks::Now();
    page_state.has_recorded_first_thread_context_library_update = false;
  }
  UpdateImplicitTabContext();
}

void ContextualTasksWebContentsUserData::HandleOpenLinkInSidePanelMode(
    std::string_view url) {
  GURL target_url(url);
  // Only accept valid URLs that are HTTP or HTTPS.
  if (!target_url.is_valid() || !target_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  content::WebContents* web_contents = &GetWebContents();
  auto* ui_service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!ui_service) {
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  BrowserWindowInterface* browser = GetBrowserWindowInterface();

  base::Uuid task_id = GetEffectiveTaskId().value_or(base::Uuid());

  ui_service->OnThreadLinkClicked(
      target_url, task_id, tab ? tab->GetWeakPtr() : nullptr,
      browser ? browser->GetWeakPtr() : nullptr,
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

void ContextualTasksWebContentsUserData::
    HandleThreadContextLibraryUpdateFromAim(
        const lens::SearchToClientMessage::UpdateThreadContextLibrary&
            message) {
  auto& page_state = GetPrimaryPageSearchState();
  if (!page_state.has_recorded_first_thread_context_library_update &&
      page_state.handshake_complete_time.has_value()) {
    page_state.has_recorded_first_thread_context_library_update = true;
    base::UmaHistogramMediumTimes(
        "ContextualTasks.ThreadContextLibrary.TimeFromHandshakeToFirstUpdate",
        base::TimeTicks::Now() - *page_state.handshake_complete_time);
  }

  if (!base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksContextLibrary)) {
    base::UmaHistogramEnumeration(
        "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
        ThreadContextLibraryUpdateOutcome::kFeatureDisabled);
    return;
  }
  contextual_search::ContextualSearchSessionHandle* session_handle =
      GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    base::UmaHistogramEnumeration(
        "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
        ThreadContextLibraryUpdateOutcome::kNoSessionHandle);
    return;
  }

  std::vector<contextual_search::FileInfo> submitted_context =
      session_handle->GetSubmittedContextFileInfos();

  std::vector<contextual_tasks::UrlResource> committed_context =
      contextual_tasks::ConvertAiModeContextToUrlResources(message,
                                                           submitted_context);
  if (committed_context.empty()) {
    base::UmaHistogramEnumeration(
        "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
        ThreadContextLibraryUpdateOutcome::kEmptyConvertedContext);
    return;
  }

  // Save the thread's tabs in the session handle's central restored tabs
  // tracker. Restored tabs are only kept while their tab is open, so resolve
  // each one to an open tab in this window and drop the ones that were closed.
  const std::vector<OpenTabInfo> open_tabs =
      GetOpenTabs(GetBrowserWindowInterface());
  const auto find_open_tab =
      [&open_tabs](int32_t session_id) -> const OpenTabInfo* {
    auto it =
        std::ranges::find_if(open_tabs, [session_id](const OpenTabInfo& tab) {
          return tab.session_id == session_id;
        });
    return it != open_tabs.end() ? &*it : nullptr;
  };
  size_t tab_context_count = 0;
  std::vector<contextual_search::TabInfo> restored_tabs;
  for (const auto& resource : committed_context) {
    if (!resource.has_chrome_tab_data) {
      continue;
    }
    ++tab_context_count;
    const std::string title = resource.title.value_or("");
    std::optional<int32_t> tab_id;
    // The tab matched to this session's submitted context.
    if (resource.tab_id.has_value() && find_open_tab(resource.tab_id->id())) {
      tab_id = resource.tab_id->id();
    }
    // Any open tab showing the same page. This covers reopened threads and
    // later updates, which cannot match submitted context since its tokens are
    // cleared below.
    if (!tab_id.has_value()) {
      auto open_tab =
          std::ranges::find_if(open_tabs, [&](const OpenTabInfo& tab) {
            return session_handle->AreUrlsEquivalent(resource.url, title,
                                                      tab.url, tab.title);
          });
      if (open_tab != open_tabs.end()) {
        tab_id = open_tab->session_id;
      }
    }
    // The tab was closed, so it is no longer part of the context library.
    if (!tab_id.has_value()) {
      continue;
    }
    // Deselections are only tracked locally until sent to the server on the
    // next query submission, so deselected tabs are skipped if in AIM thread
    // updates.
    if (const OpenTabInfo* open_tab = find_open_tab(*tab_id);
        open_tab &&
        session_handle->IsTabDeselected(SessionID::FromSerializedValue(*tab_id),
                                        open_tab->url, open_tab->title)) {
      continue;
    }
    contextual_search::TabInfo tab;
    tab.tab_id = *tab_id;
    tab.url = resource.url;
    tab.title = title;
    tab.submitted = true;

    restored_tabs.push_back(std::move(tab));
  }
  const size_t committed_context_count = committed_context.size();
  const size_t restored_tab_count = restored_tabs.size();
  session_handle->SetRestoredTabs(std::move(restored_tabs));

  // The submitted contexts are now part of the task's server-provided
  // context (and the restored tabs above), so the session handle no longer
  // needs to track their tokens.
  session_handle->ClearSubmittedContextTokens();

  // Save the server-reported URL resources only if task ID exists to trigger
  // recomputation of tab strip underlines. Task ID might not exist since this
  // postMessage (that triggered this function) can arrive before `mtid` is in
  // the URL and a task is created. However, restored tabs etc. should be set
  // when this postMessage is sent, even if task ID does not exist.
  const std::optional<base::Uuid> task_id = GetEffectiveTaskId();
  if (task_id.has_value()) {
    if (auto* service =
            contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
                Profile::FromBrowserContext(
                    GetWebContents().GetBrowserContext()))) {
      service->SetUrlResourcesFromServer(*task_id,
                                         std::move(committed_context));
    }
  }

  base::UmaHistogramEnumeration(
      "ContextualTasks.ThreadContextLibrary.UpdateOutcome",
      task_id.has_value()
          ? ThreadContextLibraryUpdateOutcome::kSuccessWithTaskId
          : ThreadContextLibraryUpdateOutcome::kSuccessWithoutTaskId);
  base::UmaHistogramCounts100(
      "ContextualTasks.ThreadContextLibrary.CommittedContextCount",
      committed_context_count);
  base::UmaHistogramCounts100(
      "ContextualTasks.ThreadContextLibrary.CommittedTabContextCount",
      tab_context_count);
  base::UmaHistogramCounts100(
      "ContextualTasks.ThreadContextLibrary.RestoredTabCount",
      restored_tab_count);

  // The restored tabs may be the only tabs in the session (e.g. when reopening
  // a previous thread), so mount the context library if it is not mounted.
  UpdateContextLibraryInputState();
}

void ContextualTasksWebContentsUserData::OnLensThumbnailCreated(
    const std::string& thumbnail_uri) {
  auto model = GetOrCreateInputStateModel();
  if (!model) {
    return;
  }
  model->SetLensCrop(thumbnail_uri);
}

void ContextualTasksWebContentsUserData::RemoveLensCrop() {
  auto model = GetOrCreateInputStateModel();
  if (!model || !model->lens_crop().has_value()) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("ContextualTasks.LensRegionChip.Dismissed"));
  base::UmaHistogramBoolean("ContextualTasks.LensRegionChip.Dismissed", true);

#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller = GetLensSearchController()) {
    if (auto* overlay = controller->lens_overlay_controller()) {
      overlay->ClearRegionSelection();
    }
    controller->CloseLensAsync(
        lens::LensOverlayDismissalSource::kContextualTasksLensChipRemoved);
  }
#endif

  model->RemoveLensCrop();
}

void ContextualTasksWebContentsUserData::OnInputStateChanged(
    const omnibox::InputState& state) {
  if (!subscribed_model_) {
    return;
  }

  const auto& crop = subscribed_model_->lens_crop();
  bool has_crop = crop.has_value();

  auto& page_state = GetPrimaryPageSearchState();
  if (has_crop && crop->data_uri != page_state.last_lens_crop_data_uri) {
    page_state.last_lens_crop_data_uri = crop->data_uri;
    for (const auto& frame : extension_frames_) {
      if (frame.is_page_bound && frame.on_lens_crop_updated_cb) {
        frame.on_lens_crop_updated_cb.Run(GURL(crop->data_uri));
      }
    }
  } else if (!has_crop) {
    page_state.last_lens_crop_data_uri.clear();
  }

  if (page_state.is_lens_crop_mounted != has_crop) {
    page_state.is_lens_crop_mounted = has_crop;
    SendInjectChromeInput(InjectedInputType::kLensChip,
                          /*is_active=*/has_crop);
    UpdateContextState();
  }
}

void ContextualTasksWebContentsUserData::OnSessionChanged(
    contextual_search::ContextualSearchSessionHandle* session_handle) {
  if (MaybeRefreshTabContextSubscription(session_handle)) {
    UpdateContextLibraryInputState();
  }
}

bool ContextualTasksWebContentsUserData::MaybeRefreshTabContextSubscription(
    contextual_search::ContextualSearchSessionHandle* session_handle) {
  if (subscribed_session_handle_.get() == session_handle &&
      !subscribed_session_handle_.WasInvalidated()) {
    return false;
  }
  if (!session_handle) {
    subscribed_session_handle_ = nullptr;
    tab_context_subscription_ = {};
    return true;
  }
  subscribed_session_handle_ = session_handle->AsWeakPtr();
  // Safe to use base::Unretained(this) because `tab_context_subscription_` is
  // owned by `this` and unsubscribes on destruction.
  tab_context_subscription_ =
      session_handle->SubscribeTabContext(base::BindRepeating(
          &ContextualTasksWebContentsUserData::OnTabContextUpdated,
          base::Unretained(this)));
  return true;
}

void ContextualTasksWebContentsUserData::OnTabContextUpdated(
    const contextual_search::TabContextState& /*state*/) {
  UpdateContextLibraryInputState();
}

void ContextualTasksWebContentsUserData::ObserveContextController(
    contextual_search::ContextualSearchContextController* controller) {
  if (observed_controller_.get() == controller) {
    return;
  }
  if (observed_controller_) {
    observed_controller_->RemoveObserver(this);
    observed_controller_ = nullptr;
  }
  if (controller) {
    if (auto weak_controller = controller->AsWeakPtr()) {
      controller->AddObserver(this);
      observed_controller_ = std::move(weak_controller);
    }
  }
}

ContextualTasksWebContentsUserData::ContextState
ContextualTasksWebContentsUserData::ComputeContextState() {
  bool any_uploading = false;
  bool any_ready = GetPrimaryPageSearchState().is_lens_crop_mounted;
  for (const auto& tab : GetSelectedTabs()) {
    if (pending_context_uploads_.contains(tab.context_token)) {
      any_uploading = true;
    } else if (!failed_context_uploads_.contains(tab.context_token)) {
      any_ready = true;
    }
  }
  if (any_uploading) {
    return ContextState::kUploading;
  }
  if (any_ready) {
    return ContextState::kReady;
  }
  return ContextState::kNone;
}

void ContextualTasksWebContentsUserData::UpdateContextState(bool force_send) {
  ContextState current_state = ComputeContextState();
  auto& page_state = GetPrimaryPageSearchState();
  if (!force_send && current_state == page_state.last_sent_context_state) {
    return;
  }
  page_state.last_sent_context_state = current_state;

  lens::ClientToSearchMessage message;
  auto* state_changed = message.mutable_on_context_state_changed();
  switch (current_state) {
    case ContextState::kNone:
      state_changed->set_context_state(
          lens::ClientToSearchMessage::OnContextStateChanged::
              CONTEXT_STATE_NONE);
      break;
    case ContextState::kUploading:
      state_changed->set_context_state(
          lens::ClientToSearchMessage::OnContextStateChanged::
              CONTEXT_STATE_UPLOADING);
      break;
    case ContextState::kReady:
      state_changed->set_context_state(
          lens::ClientToSearchMessage::OnContextStateChanged::
              CONTEXT_STATE_READY);
      break;
  }
  PostSearchMessage(message);
}

std::vector<lens::AddedContext>
ContextualTasksWebContentsUserData::ComputeSuggestInputContexts() {
  if (!subscribed_session_handle_) {
    return {};
  }

  const auto selected_tabs = GetSelectedTabs();
  if (selected_tabs.empty()) {
    return {};
  }

  const std::optional<base::UnguessableToken> overlay_token =
      GetLensOverlayToken();
  std::vector<lens::AddedContext> contexts;
  for (const auto& file_info :
       subscribed_session_handle_->GetUploadedContextFileInfos()) {
    if (!file_info.request_id.has_value()) {
      continue;
    }
    if (overlay_token.has_value() && file_info.file_token == *overlay_token) {
      continue;
    }
    if (file_info.is_implicit_upload ||
        (file_info.input_data && file_info.input_data->is_implicit_upload)) {
      continue;
    }
    if (pending_context_uploads_.contains(file_info.file_token) ||
        failed_context_uploads_.contains(file_info.file_token)) {
      continue;
    }
    if (std::ranges::none_of(
            selected_tabs, [&](const contextual_search::TabInfo& selected_tab) {
              return !selected_tab.pending_delayed_upload &&
                     selected_tab.context_token == file_info.file_token;
            })) {
      continue;
    }

    lens::AddedContext added;
    added.set_search_session_id(
        subscribed_session_handle_->search_session_id());
    *added.mutable_request_id() = *file_info.request_id;
    if (file_info.input_data && file_info.input_data->upload_type.has_value()) {
      added.set_contextual_input_upload_type(
          *file_info.input_data->upload_type);
    } else {
      added.set_contextual_input_upload_type(
          lens::LensOverlayContextualInputUploadType::
              CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
    }
    contexts.push_back(std::move(added));
  }
  return contexts;
}

void ContextualTasksWebContentsUserData::UpdateSuggestInputs() {
  if (!IsContextualTasksTabBasedSuggestionsEnabled()) {
    return;
  }

  lens::ClientToSearchMessage message;
  auto* suggest_inputs = message.mutable_on_suggest_inputs_changed();
  for (auto& context : ComputeSuggestInputContexts()) {
    *suggest_inputs->add_added_contexts() = std::move(context);
  }

  std::string serialized;
  suggest_inputs->SerializeToString(&serialized);
  auto& page_state = GetPrimaryPageSearchState();
  if (serialized == page_state.last_sent_suggest_inputs_serialized) {
    return;
  }
  page_state.last_sent_suggest_inputs_serialized = std::move(serialized);
  PostSearchMessage(message);
}

contextual_search::ContextualSearchSessionHandle*
ContextualTasksWebContentsUserData::GetExistingContextualSessionHandle() {
  auto* helper =
      ContextualSearchWebContentsHelper::FromWebContents(&GetWebContents());
  if (!helper) {
    return nullptr;
  }
  return task_id_.has_value() ? helper->GetSessionForTask(*task_id_)
                              : helper->session_handle();
}

contextual_search::ContextualSearchSessionHandle*
ContextualTasksWebContentsUserData::GetOrCreateContextualSessionHandle() {
  content::WebContents* web_contents = &GetWebContents();
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents);

  contextual_search::ContextualSearchSessionHandle* existing_session =
      task_id_.has_value() ? helper->GetSessionForTask(task_id_.value())
                           : helper->session_handle();
  if (existing_session) {
    ObserveContextController(existing_session->GetController());
    return existing_session;
  }

  if (!task_id_) {
    auto* browser_context = web_contents->GetBrowserContext();
    Profile* profile = Profile::FromBrowserContext(browser_context);
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile);
    if (contextual_search_service) {
      auto session_handle = contextual_search_service->CreateSession(
          contextual_tasks::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kContextualTasks,
          lens::LensOverlayInvocationSource::kContextualTasksComposebox);
      session_handle->CheckSearchContentSharingSettings(profile->GetPrefs());
      helper->SetTaskSession(std::nullopt, std::move(session_handle),
                             /*input_state_model=*/nullptr);
      auto* created_session = helper->session_handle();
      if (created_session) {
        ObserveContextController(created_session->GetController());
      }
      return created_session;
    }
  }

  return existing_session;
}

std::optional<base::UnguessableToken>
ContextualTasksWebContentsUserData::GetLensOverlayToken() {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller = GetLensSearchController()) {
    auto* overlay = controller->lens_overlay_controller();
    if (!overlay || !overlay->HasRegionSelection()) {
      return std::nullopt;
    }
    if (auto* router = controller->query_router()) {
      return router->overlay_tab_context_file_token();
    }
  }
#endif
  return std::nullopt;
}

#if !BUILDFLAG(IS_ANDROID)
LensSearchController*
ContextualTasksWebContentsUserData::GetLensSearchController() const {
  auto* browser_window_interface = GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return nullptr;
  }
  auto* active_tab = browser_window_interface->GetActiveTabInterface();
  if (!active_tab || !active_tab->GetContents()) {
    return nullptr;
  }
  return LensSearchController::FromTabWebContents(active_tab->GetContents());
}
#endif

void ContextualTasksWebContentsUserData::DoSubmitQueryCleanup() {
  GetPrimaryPageSearchState().is_zero_state = false;
  implicit_tab_.reset();
  pending_context_uploads_.clear();
  failed_context_uploads_.clear();
  if (auto model = GetOrCreateInputStateModel()) {
    model->RemoveLensCrop();
  }

  RefreshActiveTaskContext();

  CloseLensAsync(
      lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted);
  UpdateContextState();
  UpdateSuggestInputs();
}

void ContextualTasksWebContentsUserData::CloseLensAsync(
    lens::LensOverlayDismissalSource dismissal_source) {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* controller = GetLensSearchController()) {
    controller->CloseLensAsync(dismissal_source);
  }
#endif
}

std::vector<contextual_search::TabInfo>
ContextualTasksWebContentsUserData::GetSelectedTabs() {
  if (!subscribed_session_handle_) {
    return {};
  }
  const auto& attached_tabs =
      subscribed_session_handle_->GetTabContextState().attached;
  const auto uploaded_tokens =
      subscribed_session_handle_->GetUploadedContextTokens();
  std::vector<contextual_search::TabInfo> selected_tabs;
  std::ranges::copy_if(attached_tabs, std::back_inserter(selected_tabs),
                       [&](const contextual_search::TabInfo& tab) {
                         return tab.pending_delayed_upload ||
                                std::ranges::contains(uploaded_tokens,
                                                      tab.context_token);
                       });
  return selected_tabs;
}

void ContextualTasksWebContentsUserData::DeleteContext(
    const base::UnguessableToken& file_token) {
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    const std::vector<contextual_search::TabInfo> selected_tabs =
        GetSelectedTabs();
    const std::optional<int32_t> session_tab_id =
        session_handle->DeleteTabContext(file_token);
    // `DeleteTabContext()` also deletes the tab's current token, if different.
    OnTabContextRemoved(file_token);
    for (const auto& tab : selected_tabs) {
      if (tab.context_token == file_token ||
          (session_tab_id.has_value() && tab.tab_id == session_tab_id)) {
        OnTabContextRemoved(tab.context_token);
        if (implicit_tab_ && implicit_tab_->token == tab.context_token) {
          implicit_tab_.reset();
        }
        if (tab.url.is_valid()) {
          dismissed_implicit_urls_.insert(tab.url);
        }
      }
    }
#if !BUILDFLAG(IS_ANDROID)
    RemoveAttachedTabUnderline(session_tab_id, *this,
                               GetBrowserWindowInterface());
#endif
  }
  if (implicit_tab_ && implicit_tab_->token == file_token) {
    implicit_tab_.reset();
  }
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::DeleteTabContext(int32_t tab_id) {
  std::optional<int32_t> session_tab_id;
  tabs::TabInterface* const tab = tabs::TabHandle(tab_id).Get();
  if (tab && tab->GetContents()) {
    SessionID session_id =
        sessions::SessionTabHelper::IdForTab(tab->GetContents());
    if (session_id.is_valid()) {
      session_tab_id = session_id.id();
    }
  }
  if (!session_tab_id.has_value()) {
    session_tab_id = GetSessionTabIdForTabHandle(tab_id);
  }
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_tab_id.has_value() || !session_handle) {
    return;
  }

  const auto matches_tab = [&](const contextual_search::TabInfo& tab_info) {
    return tab_info.tab_id == *session_tab_id;
  };

  // Search every attached and restored tab, not just unsubmitted ones, so that
  // tabs from earlier turns can be deselected too.
  const contextual_search::TabContextState& tab_context =
      session_handle->GetTabContextState();
  // Compare all attached tabs to see if any match `session_tab_id`.
  auto attached_tab = std::ranges::find_if(tab_context.attached, matches_tab);

  // Cannot use 'is_restored_from_aim' to check if it is a restored tab
  // since `session_tab_id`, the tab that is being looked for, is a tab id,
  // not `TabInfo`.
  auto restored_tab = std::ranges::find_if(tab_context.restored, matches_tab);
  const bool is_attached = attached_tab != tab_context.attached.end();
  const bool is_restored = restored_tab != tab_context.restored.end();
  if (!is_attached && !is_restored) {
    return;
  }
  const bool is_submitted =
      is_attached ? attached_tab->submitted : restored_tab->submitted;
  // The server only drops a submitted tab when tab deselection is enabled, so
  // otherwise keep showing it rather than hiding context the server still uses.
  if (is_submitted && !IsTabDeselectionEnabled()) {
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  contextual_tasks::RemoveTabUnderline(tab_id, GetBrowserWindowInterface());
#endif
  // Remove attached and restored tabs.
  GURL current_url;
  std::string current_title;
  if (tab && tab->GetContents()) {
    current_url = tab->GetContents()->GetLastCommittedURL();
    current_title = base::UTF16ToUTF8(tab->GetContents()->GetTitle());
  }
  if (current_url.is_valid()) {
    dismissed_implicit_urls_.insert(current_url);
  }
  if (std::optional<std::vector<base::UnguessableToken>> removed_tokens =
          session_handle->DeleteTabContext(
              SessionID::FromSerializedValue(*session_tab_id), current_url,
              current_title)) {
    for (const auto& token : *removed_tokens) {
      OnTabContextRemoved(token);
      if (implicit_tab_ && implicit_tab_->token == token) {
        implicit_tab_.reset();
      }
    }
  }
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

bool ContextualTasksWebContentsUserData::OnBrowserTabClosedOrNavigated(
    SessionID tab_session_id,
    std::optional<GURL> current_url,
    std::optional<std::string> current_title) {
  auto* session_handle = GetExistingContextualSessionHandle();
  if (!session_handle) {
    return false;
  }
  std::vector<GURL> candidate_urls_to_detach;
  auto record_candidate_url = [&](const GURL& url) {
    if (url.is_valid() &&
        !std::ranges::contains(candidate_urls_to_detach, url)) {
      candidate_urls_to_detach.push_back(url);
    }
  };
  for (const auto& tab : session_handle->GetTabContextState().attached) {
    if (tab.tab_id == tab_session_id.id()) {
      record_candidate_url(tab.url);
    }
  }
  for (const auto& tab : session_handle->GetTabContextState().restored) {
    if (tab.tab_id == tab_session_id.id()) {
      record_candidate_url(tab.url);
    }
  }
  for (const auto& file_info : session_handle->GetSubmittedContextFileInfos()) {
    if (file_info.tab_session_id == tab_session_id &&
        file_info.tab_url.has_value()) {
      record_candidate_url(*file_info.tab_url);
    }
  }

  const size_t attached_before =
      session_handle->GetTabContextState().attached.size();
  const size_t restored_before =
      session_handle->GetTabContextState().restored.size();
  std::vector<base::UnguessableToken> removed_tokens =
      session_handle->OnTabClosedOrNavigated(
          tab_session_id, std::move(current_url), std::move(current_title));
  const bool state_changed =
      !removed_tokens.empty() ||
      session_handle->GetTabContextState().attached.size() != attached_before ||
      session_handle->GetTabContextState().restored.size() != restored_before;
  if (!state_changed) {
    return false;
  }
  if (std::optional<base::Uuid> task_id = GetEffectiveTaskId()) {
    if (auto* service =
            contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
                Profile::FromBrowserContext(
                    GetWebContents().GetBrowserContext()))) {
      const auto& state_after = session_handle->GetTabContextState();
      for (const GURL& url : candidate_urls_to_detach) {
        const bool still_referenced =
            std::ranges::any_of(state_after.attached,
                                [&](const contextual_search::TabInfo& tab) {
                                  return tab.url == url;
                                }) ||
            std::ranges::any_of(state_after.restored,
                                [&](const contextual_search::TabInfo& tab) {
                                  return tab.url == url;
                                });
        if (!still_referenced) {
          service->DetachUrlFromTask(*task_id, url);
        }
      }
    }
  }
  for (const auto& token : removed_tokens) {
    OnTabContextRemoved(token);
    if (implicit_tab_ && implicit_tab_->token == token) {
      implicit_tab_.reset();
    }
  }
  if (subscribed_model_) {
    subscribed_model_->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  return true;
}

void ContextualTasksWebContentsUserData::ClearFiles(
    bool should_block_auto_suggested_tabs) {
#if !BUILDFLAG(IS_ANDROID)
  auto* browser_window_interface = GetBrowserWindowInterface();
#endif
  for (const auto& selected_tab : GetSelectedTabs()) {
    if (should_block_auto_suggested_tabs && implicit_tab_ &&
        selected_tab.context_token == implicit_tab_->token &&
        selected_tab.url.is_valid()) {
      dismissed_implicit_urls_.insert(selected_tab.url);
    }
#if !BUILDFLAG(IS_ANDROID)
    RemoveAttachedTabUnderline(selected_tab.tab_id, *this,
                               browser_window_interface);
#endif
  }
  implicit_tab_.reset();
  pending_context_uploads_.clear();
  failed_context_uploads_.clear();
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_handle->ClearFiles();
  }
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::ClearInputPlateState() {
  submit_pending_ = false;
  preserve_prestaged_context_ = false;
  RemoveLensCrop();
  auto& page_state = GetPrimaryPageSearchState();
  page_state.is_zero_state = false;
  page_state.last_lens_crop_data_uri.clear();
  if (page_state.is_lens_crop_mounted) {
    page_state.is_lens_crop_mounted = false;
    SendInjectChromeInput(InjectedInputType::kLensChip, /*is_active=*/false);
  }
  dismissed_implicit_urls_.clear();
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
#if !BUILDFLAG(IS_ANDROID)
    auto* browser_window_interface = GetBrowserWindowInterface();
    for (const auto& attached_tab :
         session_handle->GetTabContextState().attached) {
      RemoveAttachedTabUnderline(attached_tab.tab_id, *this,
                                 browser_window_interface);
    }
#endif
    session_handle->ClearSubmittedContextTokens();
    session_handle->SetPersistedTabs({});
    session_handle->set_deselected_tabs_urls({});
    session_handle->SetRestoredTabs({});
  }
  ClearFiles();
}

void ContextualTasksWebContentsUserData::UpdateImplicitTabContext() {
  BrowserWindowInterface* browser_window = GetBrowserWindowInterface();
  auto* controller = browser_window
                         ? ContextualTasksPanelController::From(browser_window)
                         : nullptr;
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  auto* tab_list =
      browser_window ? TabListInterface::From(browser_window) : nullptr;
  tabs::TabInterface* active_tab =
      tab_list ? tab_list->GetActiveTab() : nullptr;
  content::WebContents* tab_contents =
      active_tab ? active_tab->GetContents() : nullptr;
  const SessionID session_id =
      tab_contents ? sessions::SessionTabHelper::IdForTab(tab_contents)
                   : SessionID::InvalidValue();
  const GURL last_committed_url =
      tab_contents ? tab_contents->GetLastCommittedURL() : GURL();
  SiteExclusionDetail site_exclusion_detail;
  contextual_search::ContextualSearchSessionHandle* session_handle = nullptr;
  if (!GetPrimaryPageSearchState().is_zero_state || !controller ||
      !ContextualTasksPanelController::IsWebContentsInPanel(
          &GetWebContents()) ||
      !controller->IsPanelOpenForContextualTask() ||
      controller->GetActiveWebContents() != &GetWebContents() ||
      !GetIsTabAutoSuggestionChipEnabled() || !CanShareTabContext(profile) ||
      !session_id.is_valid() ||
      !IsValidUrlForSuggestedTab(last_committed_url, profile,
                                 site_exclusion_detail) ||
      dismissed_implicit_urls_.contains(last_committed_url) ||
      !(session_handle = GetOrCreateContextualSessionHandle())) {
    RemoveImplicitTabContext();
    return;
  }

  const tabs::TabHandle tab_handle = active_tab->GetHandle();
  const std::string tab_title = base::UTF16ToUTF8(tab_contents->GetTitle());
  const std::vector<contextual_search::TabInfo> selected_tabs =
      GetSelectedTabs();
  auto existing_it = std::ranges::find(selected_tabs, session_id.id(),
                                       &contextual_search::TabInfo::tab_id);
  if (existing_it != selected_tabs.end()) {
    if (implicit_tab_ && existing_it->context_token == implicit_tab_->token) {
      if (existing_it->url == last_committed_url &&
          existing_it->title == tab_title) {
        return;
      }
    } else {
      // Active tab was manually attached; keep it and drop any stale
      // suggestion.
      RemoveImplicitTabContext();
      return;
    }
  } else if (implicit_tab_.has_value()) {
    RemoveImplicitTabContext();
  }

  const bool is_new_suggestion = (existing_it == selected_tabs.end()) ||
                                 (existing_it->url != last_committed_url);
  RecordTabIdMapping(tab_handle.raw_value(), session_id.id());
  implicit_tab_ = ImplicitTab{
      .token = session_handle->AddPendingDelayedTab(
          session_id.id(), last_committed_url, tab_title),
      .handle = tab_handle,
  };
  if (is_new_suggestion) {
    contextual_tasks::RecordTabAddedMetric(
        active_tab, session_handle->GetMetricsRecorder(),
        /*is_tab_suggestion_chip=*/true, browser_window);
  }
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::OnManualTabContextAdded(
    const GURL& url,
    SessionID tab_session_id) {
  dismissed_implicit_urls_.erase(url);
  if (implicit_tab_ &&
      std::ranges::any_of(GetSelectedTabs(), [&](const auto& t) {
        return t.context_token == implicit_tab_->token &&
               t.tab_id == tab_session_id.id();
      })) {
    implicit_tab_.reset();
  }
}

std::optional<tabs::TabHandle>
ContextualTasksWebContentsUserData::GetAutoSuggestedTabHandle() const {
  return (implicit_tab_ && implicit_tab_->handle.Get())
             ? std::make_optional(implicit_tab_->handle)
             : std::nullopt;
}

void ContextualTasksWebContentsUserData::RemoveImplicitTabContext() {
  if (!implicit_tab_.has_value()) {
    return;
  }
  const base::UnguessableToken old_token = implicit_tab_->token;
  implicit_tab_.reset();

  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle ||
      !std::ranges::contains(GetSelectedTabs(), old_token,
                             &contextual_search::TabInfo::context_token)) {
    return;
  }
  [[maybe_unused]] const std::optional<int32_t> session_tab_id =
      session_handle->DeleteTabContext(old_token);
  OnTabContextRemoved(old_token);
#if !BUILDFLAG(IS_ANDROID)
  RemoveAttachedTabUnderline(session_tab_id, *this,
                             GetBrowserWindowInterface());
#endif
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::PreservePreStagedContext() {
  preserve_prestaged_context_ = true;
}

void ContextualTasksWebContentsUserData::AssociateTabWithTask(
    SessionID tab_session_id) {
  if (!tab_session_id.is_valid()) {
    return;
  }
  std::optional<base::Uuid> task_id = GetEffectiveTaskId();
  if (!task_id) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  if (auto* service =
          contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
              profile)) {
    service->AssociateTabWithTask(*task_id, tab_session_id);
  }
}

std::optional<base::Uuid>
ContextualTasksWebContentsUserData::GetEffectiveTaskId() const {
  if (task_id_ && task_id_->is_valid()) {
    return task_id_;
  }
  auto* helper = ContextualSearchWebContentsHelper::FromWebContents(
      &const_cast<content::WebContents&>(GetWebContents()));
  if (helper && helper->task_id() && helper->task_id()->is_valid()) {
    return helper->task_id();
  }
  return std::nullopt;
}

base::CallbackListSubscription
ContextualTasksWebContentsUserData::RegisterZeroStateCallback(
    base::RepeatingClosure callback) {
  return zero_state_callbacks_.Add(std::move(callback));
}

void ContextualTasksWebContentsUserData::RefreshActiveTaskContext() {
  auto* browser_window_interface = GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return;
  }
  if (auto* provider = contextual_tasks::ActiveTaskContextProvider::From(
          browser_window_interface)) {
    provider->RefreshContext();
  }
}

BrowserWindowInterface*
ContextualTasksWebContentsUserData::GetBrowserWindowInterface() const {
  content::WebContents* host_contents =
      &const_cast<content::WebContents&>(GetWebContents());
  // Retrieve the browser window via TabInterface when hosted in a tab, or via
  // WebUI embedding context when hosted in the side panel.
  auto* tab = tabs::TabInterface::MaybeGetFromContents(host_contents);
#if !BUILDFLAG(IS_ANDROID)
  return tab ? tab->GetBrowserWindowInterface()
             : webui::GetBrowserWindowInterface(host_contents);
#else
  return tab ? tab->GetBrowserWindowInterface() : nullptr;
#endif
}

void ContextualTasksWebContentsUserData::RecordTabIdMapping(
    int32_t tab_handle_id,
    int32_t session_tab_id) {
  tab_handle_to_session_id_[tab_handle_id] = session_tab_id;
}

std::optional<int32_t>
ContextualTasksWebContentsUserData::GetSessionTabIdForTabHandle(
    int32_t tab_handle_id) const {
  auto it = tab_handle_to_session_id_.find(tab_handle_id);
  if (it != tab_handle_to_session_id_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<int32_t>
ContextualTasksWebContentsUserData::GetTabHandleForSessionTabId(
    int32_t session_tab_id) const {
  for (const auto& [tab_handle_id, mapped_session_id] :
       tab_handle_to_session_id_) {
    if (mapped_session_id == session_tab_id) {
      return tab_handle_id;
    }
  }
  return std::nullopt;
}

std::optional<int32_t>
ContextualTasksWebContentsUserData::ResolveAndRecordTabHandleForSessionTabId(
    int32_t session_tab_id) {
  if (std::optional<int32_t> handle_id =
          GetTabHandleForSessionTabId(session_tab_id)) {
    return handle_id;
  }
  if (std::optional<int32_t> open_tab_handle_id =
          FindOpenTabHandleForSessionTabId(session_tab_id,
                                           GetBrowserWindowInterface())) {
    RecordTabIdMapping(*open_tab_handle_id, session_tab_id);
    return open_tab_handle_id;
  }
  return std::nullopt;
}

void ContextualTasksWebContentsUserData::SetContextualTasksServiceForTesting(
    ContextualTasksService* service) {
  query_contextualizer_ = std::make_unique<QueryContextualizer>(
      service, query_contextualizer_delegate_.get());
}

void ContextualTasksWebContentsUserData::
    AppendTabContextsToOnSubmitQueryResponseForTesting(
        lens::ClientToSearchMessage* response_message,
        contextual_search::ContextualSearchSessionHandle* session_handle,
        std::optional<base::UnguessableToken> overlay_token) {
  AppendTabContextsToOnSubmitQueryResponse(response_message, session_handle,
                                           overlay_token);
}

void ContextualTasksWebContentsUserData::RecordTimeToHandshakeComplete() {
#if !BUILDFLAG(IS_ANDROID)
  content::WebContents* wc = &GetWebContents();
  if (auto* browser = webui::GetBrowserWindowInterface(wc)) {
    if (auto* panel_controller =
            contextual_tasks::ContextualTasksPanelController::From(browser)) {
      panel_controller->RecordTimeToHandshakeComplete(wc);
    }
  }
#endif
}

void ContextualTasksWebContentsUserData::
    AppendTabContextsToOnSubmitQueryResponse(
        lens::ClientToSearchMessage* response_message,
        contextual_search::ContextualSearchSessionHandle* session_handle,
        const std::optional<base::UnguessableToken>& overlay_token) {
  auto* on_submit_response =
      response_message->mutable_on_submit_query_response();

  // Report persisted tabs the user removed since the last turn (deselected,
  // closed, or navigated away) before reading the uploaded tokens, since this
  // also drops tokens for closed tabs. `TakeRemovedContexts()` also carries
  // Smart Tab Sharing removals, but this surface never produces them:
  // `SetSmartTabSharingActive()` is a no-op here because the AIM page is a
  // website and must not be able to trigger tab sharing without a user
  // gesture. If Smart Tab Sharing is enabled for this surface later, that
  // gating belongs in `SetSmartTabSharingActive()` and the recent interaction
  // check in `HandleOnSubmitQueryRequest()`, not here.
  for (const auto& removed_id : session_handle->TakeRemovedContexts()) {
    *on_submit_response->add_removed_contexts()->mutable_request_id() =
        removed_id;
  }

  const auto selected_tabs = GetSelectedTabs();
  size_t explicit_tab_count = 0;
  size_t implicit_tab_count = 0;
  // Add uploaded tab contexts directly from the session handle.
  for (const auto& file_info : session_handle->GetUploadedContextFileInfos()) {
    if (!file_info.request_id.has_value()) {
      continue;
    }
    // If this file corresponds to the lens overlay token, it will be handled
    // separately when Lens crop is finalized.
    if (overlay_token.has_value() && file_info.file_token == *overlay_token) {
      continue;
    }
    if (std::ranges::none_of(
            selected_tabs, [&](const contextual_search::TabInfo& selected_tab) {
              return selected_tab.context_token == file_info.file_token;
            })) {
      continue;
    }
    auto* added = on_submit_response->add_added_contexts();
    added->set_search_session_id(session_handle->search_session_id());
    *added->mutable_request_id() = *file_info.request_id;
    const auto upload_type =
        (file_info.input_data && file_info.input_data->upload_type.has_value())
            ? *file_info.input_data->upload_type
            : lens::LensOverlayContextualInputUploadType::
                  CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;
    added->set_contextual_input_upload_type(upload_type);
    const bool is_implicit =
        file_info.is_implicit_upload ||
        (file_info.input_data && file_info.input_data->is_implicit_upload) ||
        upload_type != lens::LensOverlayContextualInputUploadType::
                           CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;
    if (is_implicit) {
      ++implicit_tab_count;
    } else {
      ++explicit_tab_count;
    }
  }
  base::UmaHistogramCounts100(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission",
      explicit_tab_count + implicit_tab_count);
  base::UmaHistogramCounts100(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission.Explicit",
      explicit_tab_count);
  base::UmaHistogramCounts100(
      "ContextualTasks.TabCoins.AttachedCountAtSubmission.Implicit",
      implicit_tab_count);
}

std::optional<lens::AddedContext>
ContextualTasksWebContentsUserData::GetLensAddedContext() {
#if !BUILDFLAG(IS_ANDROID)
  auto model = GetOrCreateInputStateModel();
  if (!model || !model->lens_crop().has_value()) {
    return std::nullopt;
  }

  auto* controller = GetLensSearchController();
  if (!controller || !controller->IsCurrentTabSameOrigin()) {
    return std::nullopt;
  }

  auto* overlay = controller->lens_overlay_controller();
  auto* query_controller = controller->lens_overlay_query_controller();
  if (!overlay || !overlay->HasRegionSelection() || !query_controller) {
    return std::nullopt;
  }

  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return std::nullopt;
  }

  // Identify the context file corresponding to the region crop across uploaded
  // and submitted context files.
  std::optional<base::UnguessableToken> overlay_token = GetLensOverlayToken();
  const contextual_search::FileInfo* file_info = nullptr;
  std::vector<contextual_search::FileInfo> uploaded_files =
      session_handle->GetUploadedContextFileInfos();
  std::vector<contextual_search::FileInfo> submitted_files =
      session_handle->GetSubmittedContextFileInfos();
  uploaded_files.insert(uploaded_files.end(), submitted_files.begin(),
                        submitted_files.end());

  if (overlay_token.has_value()) {
    for (const auto& info : uploaded_files) {
      if (info.file_token == *overlay_token) {
        file_info = &info;
        break;
      }
    }
  }
  if (!file_info) {
    for (const auto& info : uploaded_files) {
      if (info.is_implicit_upload && info.input_data &&
          info.input_data->upload_type ==
              lens::LensOverlayContextualInputUploadType::
                  CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY) {
        file_info = &info;
        break;
      }
    }
  }

  if (!file_info || !file_info->request_id.has_value()) {
    return std::nullopt;
  }

  if (!overlay_token.has_value()) {
    overlay_token = file_info->file_token;
  }

  lens::AddedContext added;

  // Search session ID.
  std::string search_session_id = session_handle->search_session_id();
  if (search_session_id.empty()) {
    search_session_id = query_controller->search_session_id();
  }
  added.set_search_session_id(search_session_id);

  // Request ID.
  *added.mutable_request_id() = *file_info->request_id;
  added.mutable_request_id()->set_media_type(
      lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);

  // Visual Search Interaction Data.
  std::optional<lens::LensOverlayVisualSearchInteractionData>
      visual_search_interaction_data;
  if (overlay_token.has_value()) {
    visual_search_interaction_data =
        session_handle->GetVisualSearchInteractionData(*overlay_token,
                                                       std::nullopt);
  }
  if (!visual_search_interaction_data.has_value()) {
    visual_search_interaction_data =
        query_controller->GetVisualSearchInteractionData();
  }
  if (!visual_search_interaction_data.has_value()) {
    visual_search_interaction_data = CreateCropInteractionData(
        overlay->selected_region(), overlay->initial_screenshot());
  }
  if (visual_search_interaction_data.has_value()) {
    *added.mutable_visual_search_interaction_data() =
        std::move(*visual_search_interaction_data);
  }

  // Contextual input upload type.
  lens::LensOverlayContextualInputUploadType upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;
  if (file_info->input_data && file_info->input_data->upload_type.has_value()) {
    upload_type = file_info->input_data->upload_type.value();
  }
  added.set_contextual_input_upload_type(upload_type);

  if (!added.has_request_id()) {
    return std::nullopt;
  }

  return added;
#else
  return std::nullopt;
#endif
}

const ContextualTasksWebContentsUserData::ExtensionFrameInfo*
ContextualTasksWebContentsUserData::FindTargetExtensionFrame() const {
  // TODO(crbug.com/573125119): Clean up per-frame handshake tracking and
  // routing fallback once Service Worker communication lands.
  // `extension_frames_` is ordered from oldest to newest registration. Prefer
  // `last_active_search_handler_id_`, then the most recently registered frame
  // that has completed its handshake, and finally fall back to the first bound
  // frame if no frame has completed handshake yet.
  if (last_active_search_handler_id_) {
    for (const auto& frame : extension_frames_) {
      if (frame.handler_id == last_active_search_handler_id_ &&
          frame.is_page_bound && frame.post_search_message_cb) {
        return &frame;
      }
    }
  }
  for (const auto& frame : base::Reversed(extension_frames_)) {
    if (frame.is_page_bound && frame.is_handshake_complete &&
        frame.post_search_message_cb) {
      return &frame;
    }
  }
  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound && frame.post_search_message_cb) {
      return &frame;
    }
  }
  return nullptr;
}

void ContextualTasksWebContentsUserData::DispatchSerializedSearchMessage(
    const std::vector<uint8_t>& message_bytes) {
  if (search_message_dispatcher_for_testing_) {
    lens::ClientToSearchMessage parsed;
    if (parsed.ParseFromArray(message_bytes.data(), message_bytes.size())) {
      search_message_dispatcher_for_testing_.Run(parsed);
    }
  }

  if (IsServiceWorkerPortConnected()) {
    auto& page_state = GetPrimaryPageSearchState();
    if (page_state.service_worker_post_message_cb) {
      page_state.service_worker_post_message_cb.Run(message_bytes);
    }
    return;
  }

  if (const ExtensionFrameInfo* target_frame = FindTargetExtensionFrame()) {
    lens::ClientToSearchMessage parsed;
    if (parsed.ParseFromArray(message_bytes.data(), message_bytes.size())) {
      target_frame->post_search_message_cb.Run(parsed);
    }
  }
}

ContextualTasksWebContentsUserData::PageSearchState&
ContextualTasksWebContentsUserData::GetPrimaryPageSearchState() {
  return *PageSearchState::GetOrCreateForPage(
      GetWebContents().GetPrimaryPage());
}

const ContextualTasksWebContentsUserData::PageSearchState*
ContextualTasksWebContentsUserData::GetPrimaryPageSearchState() const {
  return PageSearchState::GetForPage(
      const_cast<content::WebContents&>(GetWebContents()).GetPrimaryPage());
}

}  // namespace contextual_tasks
