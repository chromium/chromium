// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#endif

namespace contextual_tasks {

namespace {

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

#if !BUILDFLAG(IS_ANDROID)
void RemoveAttachedTabUnderline(
    std::optional<int32_t> session_tab_id,
    const ContextualTasksWebContentsUserData& user_data,
    BrowserWindowInterface* browser_window_interface) {
  if (!session_tab_id.has_value() || !browser_window_interface) {
    return;
  }
  if (auto* tab_list = TabListInterface::From(browser_window_interface)) {
    for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
      if (!tab || !tab->GetContents()) {
        continue;
      }
      if (sessions::SessionTabHelper::IdForTab(tab->GetContents()).id() ==
          *session_tab_id) {
        contextual_tasks::RemoveTabUnderline(tab->GetHandle().raw_value(),
                                             browser_window_interface);
        return;
      }
    }
  }
  if (std::optional<int32_t> tab_handle_id =
          user_data.GetTabHandleForSessionTabId(*session_tab_id)) {
    contextual_tasks::RemoveTabUnderline(*tab_handle_id,
                                         browser_window_interface);
  }
}
#endif

void DeleteTabToken(
    contextual_search::ContextualSearchSessionHandle* session_handle,
    const base::UnguessableToken& token) {
  if (!session_handle) {
    return;
  }
  if (!session_handle->DeleteFile(token)) {
    session_handle->RemoveUploadedContextToken(token);
  }
}

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

ContextualTasksWebContentsUserData::ContextualTasksWebContentsUserData(
    content::WebContents* contents)
    : content::WebContentsUserData<ContextualTasksWebContentsUserData>(
          *contents) {}

ContextualTasksWebContentsUserData::~ContextualTasksWebContentsUserData() =
    default;

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
  is_lens_crop_mounted_ = false;
  last_lens_crop_data_uri_.clear();
  if (subscribed_model_) {
    // Safe to use base::Unretained(this) because `input_state_subscription_` is
    // owned by `this` and unsubscribes on destruction.
    input_state_subscription_ =
        subscribed_model_->subscribe(base::BindRepeating(
            &ContextualTasksWebContentsUserData::OnInputStateChanged,
            base::Unretained(this)));
  } else {
    input_state_subscription_ = {};
  }
}

void ContextualTasksWebContentsUserData::SetTaskId(const base::Uuid& uuid) {
  task_id_ = uuid;
  // Moves to `uuid`'s session. An untasked session and its tabs are adopted by
  // `uuid` (see `GetSessionForTask()`), so do not reset its tabs. The new
  // session will adopt these unassigned tabs.
  GetOrCreateInputStateModel();
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
    bool is_page_bound) {
  UpdateExtensionFrameBound(handler_id, is_page_bound, base::NullCallback(),
                            base::NullCallback());
}

void ContextualTasksWebContentsUserData::UpdateExtensionFrameBound(
    const void* handler_id,
    bool is_page_bound,
    base::RepeatingCallback<void(const lens::ClientToSearchMessage&)>
        post_search_message_cb,
    base::RepeatingCallback<void(const GURL&)> on_lens_crop_updated_cb) {
  bool had_bound_frame = HasBoundExtensionFrame();
  for (auto& frame : extension_frames_) {
    if (frame.handler_id == handler_id) {
      frame.is_page_bound = is_page_bound;
      frame.post_search_message_cb = std::move(post_search_message_cb);
      frame.on_lens_crop_updated_cb = std::move(on_lens_crop_updated_cb);
      if (is_page_bound && !had_bound_frame) {
        is_lens_crop_mounted_ = false;
        last_lens_crop_data_uri_.clear();
      }
      return;
    }
  }
}

void ContextualTasksWebContentsUserData::UnregisterExtensionFrame(
    const void* handler_id) {
  std::erase_if(extension_frames_, [&](const ExtensionFrameInfo& frame) {
    return frame.handler_id == handler_id;
  });
}

bool ContextualTasksWebContentsUserData::IsPrimarySearchMessageSender(
    const void* handler_id) const {
  if (!handler_id) {
    return false;
  }

  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound) {
      return frame.handler_id == handler_id;
    }
  }

  return false;
}

bool ContextualTasksWebContentsUserData::HasBoundExtensionFrame() const {
  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound) {
      return true;
    }
  }
  return false;
}

void ContextualTasksWebContentsUserData::PostSearchMessage(
    const lens::ClientToSearchMessage& message) {
  for (const auto& frame : extension_frames_) {
    if (frame.is_page_bound && frame.post_search_message_cb) {
      frame.post_search_message_cb.Run(message);
      return;
    }
  }
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
  context_library_is_active_ = true;
  SendInjectChromeInput(InjectedInputType::kContextLibrary,
                        /*is_active=*/true);
}

void ContextualTasksWebContentsUserData::UpdateContextLibraryInputState() {
  bool has_tabs = !GetSelectedTabs().empty();
  if (has_tabs == context_library_is_active_) {
    return;
  }
  context_library_is_active_ = has_tabs;
  SendInjectChromeInput(InjectedInputType::kContextLibrary,
                        /*is_active=*/has_tabs);
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

  if (has_crop && crop->data_uri != last_lens_crop_data_uri_) {
    last_lens_crop_data_uri_ = crop->data_uri;
    for (const auto& frame : extension_frames_) {
      if (frame.is_page_bound && frame.on_lens_crop_updated_cb) {
        frame.on_lens_crop_updated_cb.Run(GURL(crop->data_uri));
      }
    }
  } else if (!has_crop) {
    last_lens_crop_data_uri_.clear();
  }

  if (is_lens_crop_mounted_ != has_crop) {
    is_lens_crop_mounted_ = has_crop;
    SendInjectChromeInput(InjectedInputType::kLensChip,
                          /*is_active=*/has_crop);
  }
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
      return helper->session_handle();
    }
  }

  return existing_session;
}

std::optional<int64_t>
ContextualTasksWebContentsUserData::GetActiveTabContextId() {
#if !BUILDFLAG(IS_ANDROID)
  auto* contextual_session_handle = GetOrCreateContextualSessionHandle();
  if (!contextual_session_handle) {
    return std::nullopt;
  }

  auto* browser_window_interface = GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return std::nullopt;
  }
  auto* active_tab = browser_window_interface->GetActiveTabInterface();
  if (!active_tab || !active_tab->GetContents()) {
    return std::nullopt;
  }
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  if (!active_tab_id.is_valid()) {
    return std::nullopt;
  }

  auto file_infos = contextual_session_handle->GetUploadedContextFileInfos();
  auto submitted_file_infos =
      contextual_session_handle->GetSubmittedContextFileInfos();
  file_infos.insert(file_infos.end(), submitted_file_infos.begin(),
                    submitted_file_infos.end());
  for (const auto& file_info : file_infos) {
    if (file_info.tab_session_id &&
        file_info.tab_session_id->id() == active_tab_id.id()) {
      return file_info.GetContextId();
    }
  }
#endif
  return std::nullopt;
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

void ContextualTasksWebContentsUserData::SetTabContextSnapshot(
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (tab_context_snapshot_.has_value() &&
      tab_context_snapshot_->first != context_token) {
    DeleteTabToken(GetOrCreateContextualSessionHandle(),
                   tab_context_snapshot_->first);
  }
  tab_context_snapshot_.emplace(context_token, std::move(page_content_data));
}

void ContextualTasksWebContentsUserData::ClearTabContextSnapshotIfMatching(
    const base::UnguessableToken& token) {
  if (tab_context_snapshot_.has_value() &&
      tab_context_snapshot_->first == token) {
    tab_context_snapshot_.reset();
  }
}
#endif

void ContextualTasksWebContentsUserData::UploadSnapshotTabContextIfPresent() {
#if !BUILDFLAG(IS_ANDROID)
  if (!tab_context_snapshot_.has_value()) {
    return;
  }
  auto [context_token, page_content_data] =
      std::move(tab_context_snapshot_.value());
  tab_context_snapshot_.reset();

  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_handle->StartTabContextUploadFlow(
        context_token, std::move(page_content_data),
        contextual_tasks::CreateImageEncodingOptions());
  }
#endif
}

void ContextualTasksWebContentsUserData::DoSubmitQueryCleanup() {
  if (auto model = GetOrCreateInputStateModel()) {
    model->RemoveLensCrop();
  }

  CloseLensAsync(
      lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted);
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
  auto* session_handle = GetOrCreateContextualSessionHandle();
  if (!session_handle) {
    return {};
  }
  const auto& attached_tabs = session_handle->GetTabContextState().attached;
  const auto uploaded_tokens = session_handle->GetUploadedContextTokens();
  std::vector<contextual_search::TabInfo> selected_tabs;
  std::ranges::copy_if(attached_tabs, std::back_inserter(selected_tabs),
                       [&](const contextual_search::TabInfo& tab) {
                         return std::ranges::contains(uploaded_tokens,
                                                      tab.context_token);
                       });
  return selected_tabs;
}

void ContextualTasksWebContentsUserData::DeleteContext(
    const base::UnguessableToken& file_token) {
#if !BUILDFLAG(IS_ANDROID)
  ClearTabContextSnapshotIfMatching(file_token);
  for (const auto& selected_tab : GetSelectedTabs()) {
    if (selected_tab.context_token == file_token) {
      RemoveAttachedTabUnderline(selected_tab.tab_id, *this,
                                 GetBrowserWindowInterface());
      break;
    }
  }
#endif
  DeleteTabToken(GetOrCreateContextualSessionHandle(), file_token);
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::DeleteTabContext(int32_t tab_id) {
  std::optional<int32_t> session_tab_id;
  if (tabs::TabInterface* const tab = tabs::TabHandle(tab_id).Get();
      tab && tab->GetContents()) {
    SessionID session_id =
        sessions::SessionTabHelper::IdForTab(tab->GetContents());
    if (session_id.is_valid()) {
      session_tab_id = session_id.id();
    }
  }
  if (!session_tab_id.has_value()) {
    session_tab_id = GetSessionTabIdForTabHandle(tab_id);
  }
  if (!session_tab_id.has_value()) {
    return;
  }
  auto selected_tabs = GetSelectedTabs();
  auto it = std::ranges::find_if(
      selected_tabs, [&](const contextual_search::TabInfo& tab_info) {
        return tab_info.tab_id == *session_tab_id;
      });
  if (it == selected_tabs.end()) {
    return;
  }
  base::UnguessableToken token = it->context_token;
#if !BUILDFLAG(IS_ANDROID)
  ClearTabContextSnapshotIfMatching(token);
  contextual_tasks::RemoveTabUnderline(tab_id, GetBrowserWindowInterface());
#endif
  DeleteTabToken(GetOrCreateContextualSessionHandle(), token);
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::ClearFiles() {
#if !BUILDFLAG(IS_ANDROID)
  tab_context_snapshot_.reset();
  auto* browser_window_interface = GetBrowserWindowInterface();
  for (const auto& selected_tab : GetSelectedTabs()) {
    RemoveAttachedTabUnderline(selected_tab.tab_id, *this,
                               browser_window_interface);
  }
#endif
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_handle->ClearFiles();
  }
  if (auto model = GetOrCreateInputStateModel()) {
    model->OnContextChanged();
  }
  UpdateContextLibraryInputState();
  RefreshActiveTaskContext();
}

void ContextualTasksWebContentsUserData::AssociateTabWithTask(
    SessionID tab_session_id) {
  if (!task_id_.has_value() || !tab_session_id.is_valid()) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  if (auto* service =
          contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
              profile)) {
    service->AssociateTabWithTask(*task_id_, tab_session_id);
  }
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

}  // namespace contextual_tasks
