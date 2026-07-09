// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_page_handler.h"

#include <utility>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/autofill_selection_dialog_event_handler.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_pin_candidate_provider.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/context/glic_tab_data_observer.h"
#include "chrome/browser/glic/host/context/glic_tab_favicon_observer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_annotation_manager.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/glic_skills_manager.h"
#include "chrome/browser/glic/host/glic_synthetic_trial_manager.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/page_metadata_manager.h"
#include "chrome/browser/glic/media/glic_media_link_helper.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/skills/skills_glic_mojom_util.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/aggregated_journal_file_serializer.h"
#include "components/actor/core/aggregated_journal_in_memory_serializer.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feedback/content/content_tracing_manager.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/metrics/metrics_service.h"
#include "components/optimization_guide/content/browser/page_content_metadata_observer.h"
#include "components/optimization_guide/core/model_quality/model_quality_util.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_service.h"
#include "components/skills/public/skills_types.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "pdf/buildflags.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/base_window.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#else
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"  // nogncheck
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

namespace mojo {

// Specializes a Mojo EqualsTraits to allow equality checks of SkBitmaps, so
// that `FocusedTabData` can be compared for equality. Given the unoptimized
// nature of the image comparison logic, this trait is being made available only
// within this compilation unit.
// TODO(b/426792593): avoid a glic-specific specialization here.
template <>
struct EqualsTraits<::SkBitmap> {
  static bool Equals(const ::SkBitmap& a, const ::SkBitmap& b) {
    return glic::FaviconEquals(a, b);
  }
};

}  // namespace mojo

namespace glic {

namespace {

mojom::GetContextResultPtr LogErrorAndUnwrapContextResult(
    base::OnceCallback<void(GlicGetContextFromTabError)> error_logger,
    GlicGetContextResult result) {
  if (!result.has_value()) {
    std::move(error_logger).Run(result.error().error_code);
    return mojom::GetContextResult::NewErrorReason(result.error().message);
  }
  return std::move(result.value());
}

mojom::GetImageBytesResultPtr LogErrorAndUnwrapImageBytesResult(
    base::OnceCallback<void(GlicGetContextFromTabError)> error_logger,
    GlicGetImageBytesResult result) {
  if (!result.has_value()) {
    std::move(error_logger).Run(result.error().error_code);
    return mojom::GetImageBytesResult::NewErrorReason(result.error().message);
  }

  auto converted_result = mojom::ImageBytesResult::New();
  converted_result->bytes = std::move(result.value()->image_bytes);

  converted_result->image_info = mojom::ImageInfo::New();
  const blink::mojom::AIPageContentImageInfoPtr& image_info =
      result.value()->image_info;
  if (image_info) {
    converted_result->image_info->caption = image_info->image_caption;
    if (image_info->source_origin) {
      converted_result->image_info->source_origin = image_info->source_origin;
    }
    converted_result->image_info->url = image_info->url;
    converted_result->image_info->mime_type = image_info->mime_type;
  }
  return mojom::GetImageBytesResult::NewImageBytes(std::move(converted_result));
}

GlicUnpinTrigger FromMojomUnpinTrigger(mojom::UnpinTrigger trigger) {
  switch (trigger) {
    case mojom::UnpinTrigger::kWebClientUnknown:
      return GlicUnpinTrigger::kWebClientUnknown;
    case mojom::UnpinTrigger::kCandidatesToggle:
      return GlicUnpinTrigger::kCandidatesToggle;
    case mojom::UnpinTrigger::kChip:
      return GlicUnpinTrigger::kChip;
    case mojom::UnpinTrigger::kActuation:
      return GlicUnpinTrigger::kActuation;
  }
}

// Monitors the panel state and the browser widget state. Emits an event any
// time the active state changes.
// inactive = (panel hidden) || (panel attached) && (window not active)
class ActiveStateCalculator : public PanelStateObserver {
 public:
  // Observes changes to active state.
  class Observer : public base::CheckedObserver {
   public:
    virtual void ActiveStateChanged(bool is_active) = 0;
  };

  explicit ActiveStateCalculator(Host* host) : host_(host) {
    host_->AddPanelStateObserver(this);
    PanelStateChanged(host_->GetPanelState());
    // Calculate state immediately to avoid having an outdated state before
    // calc_timer_ triggers recalculation and any observers are attached.
    RecalculateAndNotify();
  }
  ~ActiveStateCalculator() override { host_->RemovePanelStateObserver(this); }

  bool IsActive() const { return is_active_; }
  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // GlicInstanceCoordinator::StateObserver implementation.
  void PanelStateChanged(const glic::mojom::PanelState& panel_state) override {
    panel_state_kind_ = panel_state.kind;
    if (panel_state_kind_ != glic::mojom::PanelStateKind::kHidden) {
      RecalculateAndNotify();
    } else {
      // Only delay hidden state. This ensures visible state is applied
      // immediately.
      PostRecalcAndNotify();
    }
  }

 private:
  // Calls RecalculateAndNotify after a short delay. This is required to prevent
  // transient states from being emitted.
  void PostRecalcAndNotify() {
    calc_timer_.Start(
        FROM_HERE, base::Milliseconds(10),
        base::BindRepeating(&ActiveStateCalculator::RecalculateAndNotify,
                            base::Unretained(this)));
  }

  void RecalculateAndNotify() {
    if (Calculate() != is_active_) {
      is_active_ = !is_active_;
      observers_.Notify(&Observer::ActiveStateChanged, is_active_);
    }
  }

  bool Calculate() {
    // TODO(b:444463509): Implement better calculation.
    return panel_state_kind_ != glic::mojom::PanelStateKind::kHidden;
  }

  base::OneShotTimer calc_timer_;

  raw_ptr<Host> host_;
  base::ObserverList<Observer> observers_;
  glic::mojom::PanelStateKind panel_state_kind_;
  bool is_active_ = false;
};

class BrowserIsOpenCalculator : public BrowserCollectionObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void BrowserIsOpenChanged(bool browser_is_open) = 0;
  };

  explicit BrowserIsOpenCalculator(Profile* profile, Observer* observer)
      : profile_(profile) {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
    GlobalBrowserCollection::GetInstance()->ForEach(
        [this](BrowserWindowInterface* browser) {
          OnBrowserCreated(browser);
          return true;
        });
    // Don't notify observer during construction.
    observer_ = observer;
  }
  ~BrowserIsOpenCalculator() override = default;

  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    if (browser->GetProfile() == profile_) {
      UpdateBrowserCount(1);
    }
  }
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    if (browser->GetProfile() == profile_) {
      UpdateBrowserCount(-1);
    }
  }

  bool IsOpen() const { return open_browser_count_ > 0; }

 private:
  void UpdateBrowserCount(int delta) {
    bool was_open = IsOpen();
    open_browser_count_ += delta;
    bool is_open = IsOpen();
    if (was_open != is_open && observer_) {
      observer_->BrowserIsOpenChanged(is_open);
    }
  }
  // Profile outlives this class. The glic web contents is torn down along
  // with GlicKeyedService, which is tied to the profile.
  raw_ptr<Profile> profile_;
  raw_ptr<Observer> observer_ = nullptr;
  int open_browser_count_ = 0;

  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

// Does time-based debouncing and cache-based deduping of FocusedTabData
// updates.
// TODO(b/424242331): Debouncing & deduping should happen closer to where
// focused tab updates are generated.
// TODO(b/424242331): This logic should be moved to a separate file and be
// made more generic and configurable.
class DebouncerDeduper {
 public:
  using DataCallback = void(glic::mojom::FocusedTabDataPtr);

  DebouncerDeduper(base::TimeDelta debounce_delay,
                   int max_debounces,
                   base::RepeatingCallback<DataCallback> callback)
      : max_debounces_(max_debounces),
        update_callback_(callback),
        debounce_timer_(FROM_HERE,
                        debounce_delay,
                        base::BindRepeating(&DebouncerDeduper::MaybeSendUpdate,
                                            base::Unretained(this))),
        remaining_debounces_(max_debounces_) {}
  ~DebouncerDeduper() = default;

  void HandleUpdate(const glic::mojom::FocusedTabDataPtr data) {
    next_data_candidate_ = data.Clone();
    if (remaining_debounces_ > 0) {
      remaining_debounces_--;
      debounce_timer_.Reset();
    }
  }

 private:
  void MaybeSendUpdate() {
    if (next_data_candidate_ != last_sent_data_) {
      last_sent_data_ = next_data_candidate_->Clone();
      update_callback_.Run(std::move(next_data_candidate_));
    }
    next_data_candidate_ = nullptr;
    remaining_debounces_ = max_debounces_;
  }

  const int max_debounces_;
  base::RepeatingCallback<DataCallback> update_callback_;
  base::RetainingOneShotTimer debounce_timer_;
  int remaining_debounces_;
  glic::mojom::FocusedTabDataPtr last_sent_data_;
  glic::mojom::FocusedTabDataPtr next_data_candidate_;
};

}  // namespace

// WARNING: One instance of this class is created per WebUI navigated to
// chrome://glic. The design and implementation of this class, which plumbs
// events through GlicKeyedService to other components, relies on the assumption
// that there is exactly 1 WebUI instance. If this assumption is ever violated
// then many classes will break.
class GlicWebClientHandler : public glic::mojom::WebClientHandler,
                             public GlicInstanceCoordinator::StateObserver,
                             public GlicWebClientAccess,
                             public BrowserAttachObserver,
                             public ActiveStateCalculator::Observer,
                             public BrowserIsOpenCalculator::Observer {
 public:
  explicit GlicWebClientHandler(
      GlicPageHandler* page_handler,
      content::BrowserContext* browser_context,
      mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver)
      : profile_(Profile::FromBrowserContext(browser_context)),
        page_handler_(page_handler),
        glic_service_(
            GlicKeyedServiceFactory::GetGlicKeyedService(browser_context)),
        window_controller_(&glic_service_->instance_coordinator()),
        pref_service_(profile_->GetPrefs()),
        active_state_calculator_(&page_handler_->host()),
        browser_is_open_calculator_(profile_, this),
        receiver_(this, std::move(receiver)),
        annotation_manager_(
            std::make_unique<GlicAnnotationManager>(glic_service_, &host())) {
    VLOG(1) << "Glic [WebClientHandler] Constructor";
    active_state_calculator_.AddObserver(this);
  }

  ~GlicWebClientHandler() override {
    VLOG(1) << "Glic [WebClientHandler] Destructor";
    active_state_calculator_.RemoveObserver(this);
    if (web_client_) {
      Uninstall();
    }
  }

  Host& host() { return page_handler_->host(); }
  GlicSharingManagerInternal& GetSharingManagerInternal() {
    return host().GetSharingManagerInternal();
  }

  // glic::mojom::WebClientHandler implementation.
  void SwitchConversation(glic::mojom::ConversationInfoPtr info,
                          SwitchConversationCallback callback) override {
    page_handler_->host().SwitchConversation(std::move(info),
                                             std::move(callback));
  }

  void RegisterConversation(glic::mojom::ConversationInfoPtr info,
                            RegisterConversationCallback callback) override {
    page_handler_->host().RegisterConversation(std::move(info),
                                               std::move(callback));
  }

  void OpenLinkInPopup(const ::GURL& url,
                       int32_t popup_width,
                       int32_t popup_height) override {
    if (!url.SchemeIsHTTPOrHTTPS()) {
      return;
    }

    content::WebContents* parent_web_contents = page_handler_->webui_contents();
    gfx::NativeView native_view = parent_web_contents->GetContentNativeView();
    const display::Display& display =
        display::Screen::Get()->GetDisplayNearestView(native_view);
    const gfx::Rect work_area = display.work_area();

    // Calculate the center coordinates.
    const int x = work_area.x() + (work_area.width() - popup_width) / 2;
    const int y = work_area.y() + (work_area.height() - popup_height) / 2;

    std::unique_ptr<NavigateParams> params = std::make_unique<NavigateParams>(
        profile_, url, ui::PAGE_TRANSITION_LINK);
    params->disposition = WindowOpenDisposition::NEW_POPUP;
    params->opened_by_another_window = true;
    params->window_features.bounds = gfx::Rect(x, y, popup_width, popup_height);
    glic::NavigateAsync(std::move(params), base::DoNothing());
  }

  void WebClientCreated(
      ::mojo::PendingRemote<glic::mojom::WebClient> web_client,
      WebClientCreatedCallback callback) override {
    VLOG(1) << "Glic [WebClientHandler] WebClientCreated";
    web_client_.Bind(std::move(web_client));
    web_client_.set_disconnect_handler(base::BindOnce(
        &GlicWebClientHandler::WebClientDisconnected, base::Unretained(this)));

    page_metadata_manager_ =
        std::make_unique<PageMetadataManager>(profile_, web_client_.get());

    // Listen for changes to prefs.
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        prefs::kGlicMicrophoneEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicGeolocationEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicTabContextEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicClosedCaptioningEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicDefaultTabContextEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        glic::prefs::kGlicGeminiEnterpriseSettings,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    web_actuation_pref_subscription_ =
        glic_service_->enabling().RegisterOnUserEnabledActuationOnWebChanged(
            base::BindRepeating(
                &GlicWebClientHandler::OnUserEnabledActuationOnWebChanged,
                base::Unretained(this)));
    consent_subscription_ =
        glic_service_->enabling().RegisterOnConsentChanged(base::BindRepeating(
            &GlicWebClientHandler::OnConsentChanged, base::Unretained(this)));
    host().AddPanelStateObserver(this);

    if (base::FeatureList::IsEnabled(
            features::kGlicTabFocusDataDedupDebounce)) {
      const base::TimeDelta debounce_delay =
          base::Milliseconds(features::kGlicTabFocusDataDebounceDelayMs.Get());
      const int max_debounces = features::kGlicTabFocusDataMaxDebounces.Get();
      debouncer_deduper_ = std::make_unique<DebouncerDeduper>(
          debounce_delay, max_debounces,
          base::BindRepeating(
              &GlicWebClientHandler::NotifyWebClientFocusedTabChanged,
              base::Unretained(this)));
    }

    focus_changed_subscription_ =
        GetSharingManagerInternal().AddFocusedTabChangedCallback(
            base::BindRepeating(&GlicWebClientHandler::OnFocusedTabChanged,
                                base::Unretained(this)));

    pinned_tabs_changed_subscription_ =
        GetSharingManagerInternal().AddPinnedTabsChangedCallback(
            base::BindRepeating(&GlicWebClientHandler::OnPinningChanged,
                                base::Unretained(this)));

    pinned_tab_data_changed_subscription_ =
        GetSharingManagerInternal().AddPinnedTabDataChangedCallback(
            base::BindRepeating(&GlicWebClientHandler::OnPinnedTabDataChanged,
                                base::Unretained(this)));

    focus_data_changed_subscription_ =
        GetSharingManagerInternal().AddFocusedTabDataChangedCallback(
            base::BindRepeating(&GlicWebClientHandler::OnFocusedTabDataChanged,
                                base::Unretained(this)));

    system_permission_settings_observation_ =
        system_permission_settings::Observe(base::BindRepeating(
            &GlicWebClientHandler::OnOsPermissionSettingChanged,
            base::Unretained(this)));

    if (base::FeatureList::IsEnabled(features::kGlicActor)) {
      // CallbackListSubscription prevents these callbacks from being invoked
      // when this object is destructed.
      act_on_web_capability_changed_subscription_ =
          glic_service_->AddActOnWebCapabilityChangedCallback(
              base::BindRepeating(
                  &GlicWebClientHandler::NotifyActOnWebCapabilityChanged,
                  base::Unretained(this)));
    }


    auto state = glic::mojom::WebClientInitialState::New();
    PopulateGlobalClientInitialState(state.get(), profile_);

    state->panel_state = host().GetPanelState().Clone();

    state->focused_tab_data =
        CreateFocusedTabData(GetSharingManagerInternal().GetFocusedTabData());
    state->can_attach = ComputeCanAttach();
    state->panel_is_active = active_state_calculator_.IsActive();

    OnPinningChanged(GetSharingManagerInternal().GetPinnedTabs());

    state->browser_is_open = browser_is_open_calculator_.IsOpen();
    state->instance_is_active = host().instance_delegate().IsActive();

    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
    local_state_pref_change_registrar_.Add(
        prefs::kGlicLauncherHotkey,
        base::BindRepeating(&GlicWebClientHandler::OnLocalStatePrefChanged,
                            base::Unretained(this)));
#endif

    std::move(callback).Run(std::move(state));
  }

  void WebClientInitializeFailed() override {
    host().WebClientInitializeFailed(this);
  }

  void WebClientInitialized() override {
    VLOG(1) << "Glic [WebClientHandler] WebClientInitialized";
    host().SetWebClient(this);
    // If chrome://glic is opened in a tab for testing, send a synthetic open
    // signal.
    if (page_handler_->webui_contents() != host().webui_contents()) {
      mojom::PanelOpeningDataPtr panel_opening_data =
          mojom::PanelOpeningData::New();
      panel_opening_data->panel_state = host().GetPanelState().Clone();
      panel_opening_data->invocation_source =
          mojom::InvocationSource::kUnsupported;
      base::UmaHistogramBoolean("Glic.Host.OpenedInRegularTab", true);
      web_client_->NotifyPanelWillOpen(std::move(panel_opening_data),
                                       base::DoNothing());
      host().skills_manager().NotifyPanelOpenedOrActivated();
    }
  }

  void GetZeroStateSuggestionsAndSubscribe(
      bool has_active_subscription,
      mojom::ZeroStateSuggestionsOptionsPtr options,
      GetZeroStateSuggestionsAndSubscribeCallback callback) override {
    host().instance_delegate().GetZeroStateSuggestionsAndSubscribe(
        has_active_subscription, *options, std::move(callback));
  }

  void CreateTab(const ::GURL& url,
                 glic::mojom::CreateTabOptionsPtr create_options,
                 CreateTabCallback callback) override {
    bool open_in_background = create_options->open_in_background;
    std::optional<int32_t> window_id = create_options->window_id;
    if (base::FeatureList::IsEnabled(media::kMediaLinkHelpers)) {
      if (auto* tab = GetSharingManagerInternal().GetFocusedTabData().focus()) {
        const bool replaced =
            GlicMediaLinkHelper(tab->GetContents()).MaybeReplaceNavigation(url);
        base::UmaHistogramBoolean("Glic.MaybeReplaceNavigation.Result",
                                  replaced);
        if (replaced) {
          std::move(callback).Run(nullptr);
          return;
        }
      }
    }
    host().instance_delegate().CreateTab(url, open_in_background, window_id,
                                         std::move(callback));
  }

  void ActivateTabWithUrl(const ::GURL& exact_url,
                          glic::mojom::ActivateTabOptionsPtr options,
                          ActivateTabWithUrlCallback callback) override {
    tabs::TabInterface* exact_match_tab = nullptr;
    tabs::TabInterface* pattern_match_tab = nullptr;
    std::string pattern_str = options ? options->pattern : "";

    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* browser) {
          if (browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL ||
              browser->GetProfile() != profile_) {
            return true;
          }
          TabListInterface* tab_list = TabListInterface::From(browser);
          if (!tab_list) {
            return true;
          }
          for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
            if (tab->GetURL().EqualsIgnoringRef(exact_url)) {
              exact_match_tab = tab;
              return false;
            }
            if (!pattern_match_tab && !pattern_str.empty() &&
                base::MatchPattern(tab->GetURL().spec(), pattern_str)) {
              pattern_match_tab = tab;
            }
          }
          return true;
        });

    tabs::TabInterface* found_tab =
        exact_match_tab ? exact_match_tab : pattern_match_tab;

    if (found_tab) {
      BrowserWindowInterface* browser = found_tab->GetBrowserWindowInterface();
      if (browser) {
        if (browser->GetWindow()) {
          browser->GetWindow()->Activate();
        }
        if (TabListInterface* tab_list = TabListInterface::From(browser)) {
          tab_list->ActivateTab(found_tab->GetHandle());
        }
      }
      mojom::TabDataPtr tab_data = CreateTabData(found_tab);
      std::move(callback).Run(std::move(tab_data));
      return;
    }

    std::optional<int32_t> win_id =
        options ? options->fallback_window_id : std::nullopt;
    host().instance_delegate().CreateTab(
        exact_url, /*open_in_background=*/false, win_id, std::move(callback),
        /*show_side_panel=*/false);
  }

  void OpenGlicSettingsPage(mojom::OpenSettingsOptionsPtr options) override {
    std::string_view metric_suffix;
    switch (options->highlightField) {
      case mojom::SettingsPageField::kOsHotkey:
        ::glic::OpenGlicKeyboardShortcutSetting(profile_);
        metric_suffix = "OsHotkey";
        break;
      case mojom::SettingsPageField::kOsEntrypointToggle:
        ::glic::OpenGlicOsToggleSetting(profile_);
        metric_suffix = "OsEntrypointToggle";
        break;
      case mojom::SettingsPageField::kLocationPermission:
        ::glic::OpenGlicLocationSetting(profile_);
        metric_suffix = "LocationPermission";
        break;
      case mojom::SettingsPageField::kNone:  // Default value.
        ::glic::OpenGlicSettingsPage(profile_);
        metric_suffix = "Default";
        break;
    }
    base::RecordComputedAction(
        base::StrCat({"GlicSessionSettingsOpened.", metric_suffix}));
  }

  void OpenPasswordManagerSettingsPage() override {
    if (!base::FeatureList::IsEnabled(
            features::kGlicOpenPasswordManagerSettingsPageApi)) {
      return;
    }
    ::glic::OpenPasswordManagerSettingsPage(profile_);
  }

  void ClosePanel() override { host().ClosePanel(page_handler_); }

  void ClosePanelAndShutdown() override { ClosePanel(); }

  void AttachPanel() override { host().AttachPanel(page_handler_); }

  void DetachPanel() override { host().DetachPanel(page_handler_); }

  void ShowProfilePicker() override {
    glic::GlicProfileManager::GetInstance()->ShowProfilePicker();
  }

  void OnModeChange(glic::mojom::WebClientMode new_mode) override {
    glic_service_->metrics()->SetWebClientMode(new_mode);
    host().OnInteractionModeChange(page_handler_, new_mode);
  }

  void OnMicrophoneStatusChange(glic::mojom::MicrophoneStatus status) override {
    host().OnMicrophoneStatusChanged(status);
  }

  void ResizeWidget(const gfx::Size& size,
                    base::TimeDelta duration,
                    ResizeWidgetCallback callback) override {
    host().ResizePanel(page_handler_, size, duration, std::move(callback));
  }

  void GetModelQualityClientId(
      GetModelQualityClientIdCallback callback) override {
    auto* local_state = g_browser_process->local_state();
    std::string client_id =
        optimization_guide::GetOrCreateGlicModelQualityClientId(local_state);
    std::move(callback).Run(std::move(client_id));
  }

  void GetContextFromFocusedTab(
      glic::mojom::TabContextOptionsPtr options,
      GetContextFromFocusedTabCallback callback) override {
    FocusedTabData ftd = GetSharingManagerInternal().GetFocusedTabData();
    if (ftd.unfocused_tab()) {
      CHECK(!ftd.focus());
      // Fail early if the active tab is un-focusable.
      glic_service_->metrics()->LogGetContextFromFocusedTabError(
          GlicGetContextFromTabError::kPermissionDenied);
      std::move(callback).Run(
          mojom::GetContextResult::NewErrorReason("permission denied"));
      return;
    }

    tabs::TabInterface* tab = ftd.focus();
    if (tab) {
      host()
          .instance_metrics_backwards_compatibility()
          .DidRequestContextFromTab(*tab);
    }
    auto tab_handle = tab ? tab->GetHandle() : tabs::TabHandle::Null();
    GetSharingManagerInternal().GetContextFromTab(
        tab_handle, *options,
        base::BindOnce(
            &LogErrorAndUnwrapContextResult,
            base::BindOnce(&GlicMetrics::LogGetContextFromFocusedTabError,
                           base::Unretained(glic_service_->metrics())))
            .Then(std::move(callback)));
  }

  void GetContextFromTab(int32_t tab_id,
                         glic::mojom::TabContextOptionsPtr options,
                         GetContextFromTabCallback callback) override {
    // Extra activation gating is done in this function.
    GetSharingManagerInternal().GetContextFromTab(
        tabs::TabHandle(tab_id), *options,
        base::BindOnce(
            &LogErrorAndUnwrapContextResult,
            base::BindOnce(&GlicMetrics::LogGetContextFromTabError,
                           base::Unretained(glic_service_->metrics())))
            .Then(std::move(callback)));
  }

  void GetImageBytesFromTab(int32_t tab_id,
                            const std::string& document_id,
                            int32_t dom_node_id,
                            GetImageBytesFromTabCallback callback) override {
    GetSharingManagerInternal().GetImageBytes(
        tabs::TabHandle(tab_id), document_id, dom_node_id,
        base::BindOnce(
            &LogErrorAndUnwrapImageBytesResult,
            base::BindOnce(&GlicMetrics::LogGetImageBytesFromTabError,
                           base::Unretained(glic_service_->metrics())))
            .Then(std::move(callback)));
  }

  void SetMaximumNumberOfPinnedTabs(
      uint32_t num_tabs,
      SetMaximumNumberOfPinnedTabsCallback callback) override {
    uint32_t effective_max =
        GetSharingManagerInternal().SetMaxPinnedTabs(num_tabs);
    std::move(callback).Run(effective_max);
  }

  void PinTabs(const std::vector<int32_t>& tab_ids,
               mojom::PinTabsOptionsPtr options,
               PinTabsCallback callback) override {
    std::vector<tabs::TabHandle> tab_handles;
    for (auto tab_id : tab_ids) {
      tab_handles.push_back(tabs::TabHandle(tab_id));
    }
    GlicPinTrigger trigger = GlicPinTrigger::kWebClientUnknown;
    if (options) {
      switch (options->pin_trigger) {
        case mojom::PinTrigger::kWebClientUnknown:
          trigger = GlicPinTrigger::kWebClientUnknown;
          break;
        case mojom::PinTrigger::kCandidatesToggle:
          trigger = GlicPinTrigger::kCandidatesToggle;
          break;
        case mojom::PinTrigger::kAtMention:
          trigger = GlicPinTrigger::kAtMention;
          break;
        case mojom::PinTrigger::kActuation:
          trigger = GlicPinTrigger::kActuation;
          break;
      }
    }
    std::move(callback).Run(
        GetSharingManagerInternal().PinTabs(tab_handles, trigger));
  }

  void UnpinTabs(const std::vector<int32_t>& tab_ids,
                 mojom::UnpinTabsOptionsPtr options,
                 UnpinTabsCallback callback) override {
    std::vector<tabs::TabHandle> tab_handles;
    for (auto tab_id : tab_ids) {
      tab_handles.push_back(tabs::TabHandle(tab_id));
    }
    GlicUnpinTrigger trigger = GlicUnpinTrigger::kWebClientUnknown;
    if (options) {
      trigger = FromMojomUnpinTrigger(options->unpin_trigger);
    }
    std::move(callback).Run(
        GetSharingManagerInternal().UnpinTabs(tab_handles, trigger));
  }

  void UnpinAllTabs(mojom::UnpinTabsOptionsPtr options) override {
    GlicUnpinTrigger trigger = GlicUnpinTrigger::kWebClientUnknown;
    if (options) {
      trigger = FromMojomUnpinTrigger(options->unpin_trigger);
    }
    GetSharingManagerInternal().UnpinAllTabs(trigger);
  }

  void CreateActorHandler(
      mojo::PendingReceiver<mojom::ActorHandler> receiver,
      mojo::PendingRemote<mojom::ActorClient> client) override {
    host().instance_delegate().CreateActorHandler(std::move(receiver),
                                                  std::move(client));
  }

  void CreateExperimentalTriggeringClient(
      mojo::PendingRemote<mojom::ExperimentalTriggeringClient> client)
      override {
    if (auto* manager =
            host().instance_delegate().GetExperimentalTriggeringManager()) {
      manager->Bind(std::move(client));
    }
  }

  void CreateAnnotationHandler(
      mojo::PendingReceiver<mojom::AnnotationHandler> receiver) override {
    if (!base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
      receiver_.ReportBadMessage(
          "CreateAnnotationHandler cannot be called without GlicScrollTo "
          "enabled.");
      return;
    }
    annotation_manager_->Bind(std::move(receiver));
  }

  void CreateSkillsHandler(
      mojo::PendingReceiver<mojom::SkillsHandler> receiver,
      mojo::PendingRemote<mojom::SkillsClient> client) override {
    host().skills_manager().Bind(std::move(receiver), std::move(client));
  }

  void ActivateTab(int32_t tab_id) override {
    tabs::TabInterface* tab = tabs::TabHandle(tab_id).Get();
    if (!tab) {
      return;
    }
    content::WebContents* contents = tab->GetContents();
    if (!contents) {
      return;
    }

    glic_service_->metrics()->OnActivateTabFromInstance(tab);
    contents->GetDelegate()->ActivateContents(contents);
  }

  void CaptureScreenshot(CaptureScreenshotCallback callback) override {
    host().CaptureScreenshot(std::move(callback));
  }

  void CaptureRegion(mojo::PendingRemote<mojom::CaptureRegionObserver> observer,
                     mojom::CaptureRegionParamsPtr params) override {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL: CaptureRegion (b/494315475)
    std::optional<int32_t> tab_id =
        params ? std::optional<int32_t>(params->tab_id) : std::nullopt;
    mojom::TabContextOptionsPtr tab_context_options =
        params ? std::move(params->options) : nullptr;
    tabs::TabInterface* tab = nullptr;
    if (tab_id.has_value()) {
      tab = tabs::TabHandle(*tab_id).Get();
    } else {
      const FocusedTabData& focus =
          GetSharingManagerInternal().GetFocusedTabData();
      // Prioritize the focused tab, but fall back to the unfocused tab if one
      // is available. This is useful in cases where the active tab is not
      // "focusable" by Glic (e.g. chrome:// pages).
      tab = focus.is_focus() ? focus.focus() : focus.unfocused_tab();
    }
    SelectionOverlayController::CaptureRegion(tab, GetSharingManagerInternal(),
                                              std::move(observer),
                                              std::move(tab_context_options));
#else
    NOTIMPLEMENTED();
#endif
  }

  void DeleteCapturedRegion(int32_t tab_id,
                            const base::UnguessableToken& id) override {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL: CaptureRegion (b/494315475)
    tabs::TabInterface* tab = tabs::TabHandle(tab_id).Get();
    if (!tab) {
      return;
    }
    if (tab->GetProfile() != profile_) {
      return;
    }
    if (auto* web_contents = tab->GetContents()) {
      if (auto* selection_overlay_controller =
              SelectionOverlayController::FromTabWebContents(web_contents)) {
        selection_overlay_controller->DeleteRegion(id,
                                                   /*is_using_keyboard=*/false);
      }
    }
#else
    NOTIMPLEMENTED();
#endif
  }

  void SetAudioDucking(bool enabled,
                       SetAudioDuckingCallback callback) override {
    // NEEDS_ANDROID_IMPL: AudioDucking not in android build.
#if !BUILDFLAG(IS_ANDROID)
    content::RenderFrameHost* guest_frame = page_handler_->GetGuestMainFrame();
    if (!guest_frame) {
      std::move(callback).Run(false);
      return;
    }
    AudioDucker* audio_ducker =
        AudioDucker::GetOrCreateForPage(guest_frame->GetPage());
    std::move(callback).Run(enabled ? audio_ducker->StartDuckingOtherAudio()
                                    : audio_ducker->StopDuckingOtherAudio());
#else
    std::move(callback).Run(false);
#endif
  }

  void SetMinimumPanelSize(const gfx::Size& size) override {
    host().SetMinimumWidgetSize(page_handler_, size);
  }

  void SetMicrophonePermissionState(
      bool enabled,
      SetMicrophonePermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicMicrophoneEnabled, enabled);
    if (enabled) {
      base::RecordAction(
          base::UserMetricsAction("GlicMicrophonePermissionEnabled"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("GlicMicrophonePermissionDisabled"));
    }
    std::move(callback).Run();
  }

  void SetLocationPermissionState(
      bool enabled,
      SetLocationPermissionStateCallback callback) override {
#if BUILDFLAG(IS_ANDROID)
    // Glic WebUI should not set location on Android if this flag is disabled.
    // See b/523326989 for context.
    if (!base::FeatureList::IsEnabled(
            chrome::android::kGlicExperimentalLocation)) {
      std::move(callback).Run();
      return;
    }
#endif
    pref_service_->SetBoolean(prefs::kGlicGeolocationEnabled, enabled);
    if (enabled) {
      base::RecordAction(
          base::UserMetricsAction("GlicLocationPermissionEnabled"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("GlicLocationPermissionDisabled"));
    }
    std::move(callback).Run();
  }

  void SetTabContextPermissionState(
      bool enabled,
      SetTabContextPermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicTabContextEnabled, enabled);
    if (enabled) {
      base::RecordAction(
          base::UserMetricsAction("GlicTabContextPermissionEnabled"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("GlicTabContextPermissionDisabled"));
    }
    std::move(callback).Run();
  }

  void SetClosedCaptioningSetting(
      bool enabled,
      SetClosedCaptioningSettingCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicClosedCaptioningEnabled, enabled);
    if (enabled) {
      base::RecordAction(
          base::UserMetricsAction("GlicClosedCaptioningEnabled"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("GlicClosedCaptioningDisabled"));
    }
    std::move(callback).Run();
  }

  void SetActuationOnWebSetting(
      bool enabled,
      SetActuationOnWebSettingCallback callback) override {
    glic_service_->enabling().SetUserEnabledActuationOnWeb(enabled);
    base::RecordAction(
        enabled ? base::UserMetricsAction("GlicUserEnabledActuationOnWeb")
                : base::UserMetricsAction("GlicUserDisabledActuationOnWeb"));
    std::move(callback).Run();
  }

  void ShouldAllowMediaPermissionRequest(
      ShouldAllowMediaPermissionRequestCallback callback) override {
    std::move(callback).Run(
        pref_service_->GetBoolean(prefs::kGlicMicrophoneEnabled) &&
        host().IsWidgetShowing(this));
  }

  void ShouldAllowGeolocationPermissionRequest(
      ShouldAllowGeolocationPermissionRequestCallback callback) override {
#if BUILDFLAG(IS_ANDROID)
    // This should not be called when the flag is disabled, but added fallback
    // to be safe.
    if (!base::FeatureList::IsEnabled(
            chrome::android::kGlicExperimentalLocation)) {
      std::move(callback).Run(false);
      return;
    }
#endif
    std::move(callback).Run(
        pref_service_->GetBoolean(prefs::kGlicGeolocationEnabled) &&
        host().IsWidgetShowing(this));
  }

  void SetContextAccessIndicator(bool enabled) override {
    host().SetContextAccessIndicator(enabled);
  }

  void GetUserProfileInfo(GetUserProfileInfoCallback callback) override {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile_->GetPath());
    auto* identity_manager =
        IdentityManagerFactory::GetForProfileIfExists(profile_);
    if (!entry || !identity_manager) {
      std::move(callback).Run(nullptr);
      return;
    }

    // ChromeOS doesn't support multi-profile, so `entry` would not be populated
    // with the correct user information. However, all profile entries are
    // populated from IdentityManager, which is supported on all platforms.
    const auto account_info =
        identity_manager->FindExtendedAccountInfoByGaiaId(entry->GetGAIAId());

    auto result = glic::mojom::UserProfileInfo::New();

    result->display_name = account_info.GetFullName().value_or("");
    result->email = account_info.GetEmail();
    result->given_name = account_info.GetGivenName().value_or("");

    policy::ManagementService* management_service =
        policy::ManagementServiceFactory::GetForProfile(profile_);
    result->is_managed =
        management_service && management_service->IsAccountManaged();

#if BUILDFLAG(IS_CHROMEOS)
    // ChromeOS doesn't support profile, so local profile name and custom
    // profile avatar are not supported. Instead, we will just use the user
    // account avatar.
    auto icon = account_info.GetAvatarImage();
    if (icon.has_value()) {
      result->avatar_icon = icon->AsBitmap();
    }
#else
    result->local_profile_name =
        base::UTF16ToUTF8(entry->GetLocalProfileName());
    // TODO(crbug.com/382794680): Determine the correct size.
    gfx::Image icon = entry->GetAvatarIcon(512);
    if (!icon.IsEmpty()) {
      result->avatar_icon = icon.AsBitmap();
    }
#endif  //  BUILDFLAG(IS_CHROMEOS)
    std::move(callback).Run(std::move(result));
  }

  void SyncCookies(SyncCookiesCallback callback) override {
    glic_service_->GetAuthController().ForceSyncCookies(
        GlicCookieSyncTrigger::kGlicClient, std::move(callback));
  }

  void ClientErrorDialogStateChanged(
      std::optional<glic::mojom::ClientErrorDialogType> shown_dialog_type)
      override {
    if (shown_dialog_type) {
      glic_service_->GetAuthController().OnClientError();
    }
  }

  void ReportClientTransientError(
      mojo_base::mojom::AbslStatusCode status_code) override {
    glic_service_->GetAuthController().OnClientTransientError(status_code);
    base::UmaHistogramSparse("Glic.Api.Client.TransientError",
                             static_cast<int>(status_code));
  }

  void ProcessCounterAbuseVerdict(
      int32_t tab_id,
      mojom::CounterAbuseVerdictPtr verdict) override {
    if (!base::FeatureList::IsEnabled(
            features::kGlicProcessCounterAbuseVerdict)) {
      return;
    }
    if (!verdict || !verdict->sb_verdict_result) {
      base::UmaHistogramEnumeration(
          "Glic.Api.ProcessCounterAbuseVerdict.Result",
          GlicProcessCounterAbuseVerdictResult::kInvalidVerdict);
      return;
    }
    if (!verdict->sb_verdict_result->show_interstitial) {
      base::UmaHistogramEnumeration(
          "Glic.Api.ProcessCounterAbuseVerdict.Result",
          GlicProcessCounterAbuseVerdictResult::kNoInterstitialRequested);
      return;
    }

    tabs::TabInterface* tab = tabs::TabHandle(tab_id).Get();
    if (!tab) {
      return;
    }
    content::WebContents* contents = tab->GetContents();
    if (!contents) {
      return;
    }
    GURL active_url = contents->GetVisibleURL();
    if (GURL(verdict->sb_verdict_result->url) != active_url) {
      base::UmaHistogramEnumeration(
          "Glic.Api.ProcessCounterAbuseVerdict.Result",
          GlicProcessCounterAbuseVerdictResult::kUrlMismatch);
      return;
    }
    if (!g_browser_process->safe_browsing_service()) {
      return;
    }
    safe_browsing::BaseUIManager* ui_manager =
        g_browser_process->safe_browsing_service()->ui_manager().get();
    if (ui_manager) {
      security_interstitials::UnsafeResource resource;
      resource.url = GURL(verdict->sb_verdict_result->url);
      resource.original_url = GURL(verdict->sb_verdict_result->url);
      resource.threat_source = safe_browsing::ThreatSource::GLIC_COUNTER_ABUSE;

      switch (verdict->sb_verdict_result->threat_type) {
        case mojom::SbThreatType::kSocialEngineering:
          resource.threat_type =
              safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
          break;
        case mojom::SbThreatType::kMalware:
          resource.threat_type =
              safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
          break;
        case mojom::SbThreatType::kUnwantedSoftware:
          resource.threat_type =
              safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_UNWANTED;
          break;
        default:
          base::UmaHistogramEnumeration(
              "Glic.Api.ProcessCounterAbuseVerdict.Result",
              GlicProcessCounterAbuseVerdictResult::kUnsupportedThreatType);
          return;
      }

      content::RenderFrameHost* primary_main_frame =
          contents->GetPrimaryMainFrame();
      if (primary_main_frame) {
        const content::GlobalRenderFrameHostId primary_main_frame_id =
            primary_main_frame->GetGlobalId();
        resource.rfh_locator = security_interstitials::UnsafeResourceLocator::
            CreateForRenderFrameToken(
                primary_main_frame_id.child_id.value(),
                primary_main_frame->GetFrameToken().value());
      }

      if (ui_manager->IsAllowlisted(
              resource.url, resource.rfh_locator, resource.navigation_id,
              resource.threat_type, resource.threat_source)) {
        base::UmaHistogramEnumeration(
            "Glic.Api.ProcessCounterAbuseVerdict.Result",
            GlicProcessCounterAbuseVerdictResult::
                kInterstitialSkippedAllowlist);
      } else {
        base::UmaHistogramEnumeration(
            "Glic.Api.ProcessCounterAbuseVerdict.Result",
            GlicProcessCounterAbuseVerdictResult::kSuccess);
        base::UmaHistogramEnumeration(
            "Glic.Api.ProcessCounterAbuseVerdict.ThreatType",
            verdict->sb_verdict_result->threat_type);
        ui_manager->DisplayBlockingPage(resource);
      }
    }
  }

  void OnOptinImpression() override {
    host().instance_metrics().OnOptinImpression();
  }

  void OnUserInputSubmitted(mojom::WebClientMode mode) override {
    if (base::FeatureList::IsEnabled(
            features::kGlicFixTimeToFirstQueryKillSwitch)) {
      glic_service_->metrics()->OnUserInputSubmitted(mode);
    }
    glic_service_->OnUserInputSubmitted(mode);
    host().instance_metrics_backwards_compatibility().OnUserInputSubmitted(
        mode);

    // TODO(crbug.com/462769104): move this to a non-metrics API.
    GetSharingManagerInternal().OnConversationTurnSubmitted();
    host().instance_delegate().OnUserInputSubmitted(mode);
  }

  void OnContextUploadStarted() override {
    glic_service_->metrics()->OnContextUploadStarted();
  }

  void OnContextUploadCompleted() override {
    glic_service_->metrics()->OnContextUploadCompleted();
  }

  void OnReaction(mojom::MetricUserInputReactionType reaction_type) override {
    host().instance_metrics().OnReaction(reaction_type);
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnResponseStarted() override {
    host().instance_metrics_backwards_compatibility().OnResponseStarted();
    host().instance_metrics().RecordAttachedContextTabCount(
        GetSharingManagerInternal().GetNumPinnedTabs());
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnResponseStopped(mojom::OnResponseStoppedDetailsPtr details) override {
    mojom::ResponseStopCause cause = mojom::ResponseStopCause::kUnknown;
    if (details) {
      cause = details->cause;
    }
    host().instance_metrics_backwards_compatibility().OnResponseStopped(cause);
  }

  void OnSessionTerminated() override {
    glic_service_->metrics()->OnSessionTerminated();
  }

  void OnTurnCompleted(glic::mojom::WebClientModel model,
                       base::TimeDelta duration) override {
    host().instance_metrics().OnTurnCompleted(model, duration);
  }

  void OnResponseRated(bool positive) override {
    glic_service_->metrics()->OnResponseRated(positive);
  }

  void OnClosedCaptionsShown() override {
    glic_service_->metrics()->LogClosedCaptionsShown();
  }

  void OnActionSubmitted(bool is_retry) override {
    host().instance_metrics().OnActionSubmitted(is_retry);
  }


  void SetSyntheticExperimentState(const std::string& trial_name,
                                   const std::string& group_name) override {
    g_browser_process->GetFeatures()
        ->glic_synthetic_trial_manager()
        ->SetSyntheticExperimentState(trial_name, group_name);
  }

  void OpenOsPermissionSettingsMenu(ContentSettingsType type) override {
    if (type != ContentSettingsType::MEDIASTREAM_MIC &&
        type != ContentSettingsType::GEOLOCATION) {
      // This will terminate the render process.
      receiver_.ReportBadMessage(
          "OpenOsPermissionSettingsMenu received for unsupported "
          "OS permission.");
      return;
    }
    system_permission_settings::OpenSystemSettings(
        page_handler_->webui_contents(), type);
  }

  void GetOsMicrophonePermissionStatus(
      GetOsMicrophonePermissionStatusCallback callback) override {
    std::move(callback).Run(system_permission_settings::IsAllowed(
        ContentSettingsType::MEDIASTREAM_MIC));
  }

  void SubscribeToPinCandidates(
      mojom::GetPinCandidatesOptionsPtr options,
      mojo::PendingRemote<mojom::PinCandidatesObserver> observer) override {
    host().pin_candidate_provider().SubscribeToPinCandidates(
        std::move(options), std::move(observer));
  }

  void SetOnboardingCompleted() override {
    glic_service_->metrics()->OnTrustFirstOnboardingAccept();
    glic_service_->enabling().SetCompletedFre(prefs::FreStatus::kCompleted);

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
    GlicLauncherConfiguration::CheckDefaultBrowserToEnableLauncher();

    BrowserWindowInterface* browser =
        ProfileBrowserCollection::GetForProfile(profile_)->FindTabbedBrowser();
    if (auto* interface = BrowserUserEducationInterface::From(browser)) {
      interface->NotifyAdditionalConditionEvent(
          feature_engagement::events::kGlicOnboardingCompleted);
    }
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  // GlicInstanceCoordinator::StateObserver implementation.
  void PanelStateChanged(const glic::mojom::PanelState& panel_state) override {
    web_client_->NotifyPanelStateChange(panel_state.Clone());
  }

  // GlicWebClientAccess implementation.

  void FloatingPanelCanAttachChanged(bool can_attach) override {
    floating_panel_can_attach_ = can_attach;
    NotifyCanAttachChanged();
  }

  void PanelWillOpen(glic::mojom::PanelOpeningDataPtr panel_opening_data,
                     PanelWillOpenCallback done) override {
    host().SetInvocationSource(panel_opening_data->invocation_source);
    base::UmaHistogramBoolean("Glic.Host.OpenedInRegularTab", false);
    web_client_->NotifyPanelWillOpen(
        std::move(panel_opening_data),
        base::BindOnce(
            [](PanelWillOpenCallback done, GlicMetrics* metrics,
               glic::mojom::OpenPanelInfoPtr info) {
              base::UmaHistogramEnumeration("Glic.Api.NotifyPanelWillOpen",
                                            info->web_client_mode);
              metrics->SetWebClientMode(info->web_client_mode);
              std::move(done).Run(std::move(info));
            },
            std::move(done), glic_service_->metrics()));
    host().skills_manager().NotifyPanelOpenedOrActivated();
  }

  void PanelWasClosed(base::OnceClosure done) override {
    host().SetInvocationSource(mojom::InvocationSource::kUnsupported);
    web_client_->NotifyPanelWasClosed(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(done)));
  }

  void StopMicrophone(base::OnceClosure done) override {
    web_client_->StopMicrophone(std::move(done));
  }

  void ManualResizeChanged(bool resizing) override {
    web_client_->NotifyManualResizeChanged(resizing);
  }

  void NotifyAdditionalContext(mojom::AdditionalContextPtr context) override {
    web_client_->NotifyAdditionalContext(std::move(context));
  }

  void NotifyActorTaskListRowClicked(int32_t task_id) override {
    web_client_->NotifyActorTaskListRowClicked(task_id);
  }

  // BrowserAttachmentObserver implementation.
  void CanAttachToBrowserChanged(bool can_attach) override {
    NotifyCanAttachChanged();
  }
  // ActiveStateCalculator implementation.
  void ActiveStateChanged(bool is_active) override {
    if (web_client_) {
      web_client_->NotifyPanelActiveChange(is_active);
      if (is_active) {
        host().skills_manager().NotifyPanelOpenedOrActivated();
      }
    }
  }

  // BrowserIsOpenCalculator implementation.
  void BrowserIsOpenChanged(bool is_open) override {
    if (web_client_) {
      web_client_->NotifyBrowserIsOpenChanged(is_open);
    }
  }

  void GetZeroStateSuggestionsForFocusedTab(
      std::optional<bool> is_fre,
      GetZeroStateSuggestionsForFocusedTabCallback callback) override {
    if (!IsZeroStateSuggestionsEnabled()) {
      receiver_.ReportBadMessage(
          "Client should not call "
          "GetZeroStateSuggestionsForFocusedTab "
          "without the GlicZeroStateSuggestions feature enabled.");
      return;
    }

    // TODO(crbug.com/424472586): Pass supported tools to service from web
    // client.
    host().instance_delegate().FetchZeroStateSuggestions(is_fre.value_or(false),
                                                         /*supported_tools=*/{},
                                                         std::move(callback));
  }

  void MaybeRefreshUserStatus() override {
    if (!base::FeatureList::IsEnabled(features::kGlicUserStatusCheck) ||
        !features::kGlicUserStatusRefreshApi.Get()) {
      receiver_.ReportBadMessage(
          "Client should not call MaybeRefreshUserStatus without the "
          "GlicUserStatusCheck feature enabled with the refresh API.");
      return;
    }
    glic_service_->enabling().UpdateUserStatusWithThrottling();
  }

  void IsDebuggerAttached(IsDebuggerAttachedCallback callback) override {
    content::RenderFrameHost* guest_main_frame =
        page_handler_->GetGuestMainFrame();
    if (!guest_main_frame) {
      std::move(callback).Run(false);
      return;
    }
    content::WebContents* guest_web_contents =
        content::WebContents::FromRenderFrameHost(guest_main_frame);
    std::move(callback).Run(
        content::DevToolsAgentHost::IsDebuggerAttached(guest_web_contents));
  }

  void OnOsPermissionSettingChanged(ContentSettingsType content_type,
                                    bool is_blocked) {
    // Ignore other content types.
    if (content_type == ContentSettingsType::GEOLOCATION) {
      web_client_->NotifyOsLocationPermissionStateChanged(!is_blocked);
    }
  }

  void OnPinningChanged(const std::vector<tabs::TabInterface*>& pinned_tabs) {
    std::vector<glic::mojom::TabDataPtr> tab_data;
    for (tabs::TabInterface* tab : pinned_tabs) {
      tab_data.push_back(CreateTabData(tab));
    }
    web_client_->NotifyPinnedTabsChanged(std::move(tab_data));
  }

  void SubscribeToPageMetadata(
      int32_t tab_id,
      const std::vector<std::string>& names,
      SubscribeToPageMetadataCallback callback) override {
    // TODO(b/480418718): Not sure how this happens but we sometimes get here
    // with a null page_metadata_manager_. The mojo pipe is cleared on mojo
    // disconnect and destruction (which also closes the mojo pipe) so suspect
    // this is more likely because we receive the message before the
    // WebClientCreated message is received (and thus before a manager is
    // created). This is a bandaid but better than crashing the browser.
    if (page_metadata_manager_) {
      page_metadata_manager_->SubscribeToPageMetadata(tab_id, names,
                                                      std::move(callback));
    } else {
      std::move(callback).Run(/*success=*/false);
    }
  }

  void OnPinnedTabDataChanged(const TabDataChange& change) {
    if (!change.tab_data) {
      return;
    }
    web_client_->NotifyPinnedTabDataChanged(change.tab_data->Clone());
  }

  void NotifyZeroStateSuggestionsChanged(
      glic::mojom::ZeroStateSuggestionsV2Ptr suggestions,
      mojom::ZeroStateSuggestionsOptionsPtr options) {
    // Ideally, we should redesign this to avoid zss suggestions being delivered
    // when there's no client.
    if (web_client_) {
      web_client_->NotifyZeroStateSuggestionsChanged(std::move(suggestions),
                                                     std::move(options));
    }
  }

  void NotifyActOnWebCapabilityChanged(bool can_act_on_web) {
    web_client_->NotifyActOnWebCapabilityChanged(can_act_on_web);
  }

  void SubscribeToTabData(
      int32_t tab_id,
      ::mojo::PendingRemote<mojom::TabDataHandler> receiver) override {
    glic_service_->tab_data_observer().SubscribeToTabData(tab_id,
                                                          std::move(receiver));
  }

  void SubscribeToTabFavicon(
      int32_t tab_id,
      ::mojo::PendingRemote<mojom::TabFaviconHandler> receiver) override {
    glic_service_->tab_favicon_observer().SubscribeToTabFavicon(
        tab_id, std::move(receiver));
  }


  void Invoke(mojom::InvokeOptionsPtr options,
              base::OnceClosure callback) override {
    web_client_->Invoke(std::move(options), std::move(callback));
  }


 private:
  glic::mojom::GeminiEnterpriseSettingsPtr GetGeminiEnterpriseSettingsPtr()
      const {
    std::optional<glic::mojom::GeminiEnterpriseSettings>
        gemini_enterprise_settings =
            GlicEnabling::GetGeminiEnterpriseSettings(profile_);
    if (gemini_enterprise_settings.has_value()) {
      return glic::mojom::GeminiEnterpriseSettings::New(
          gemini_enterprise_settings->project_id,
          gemini_enterprise_settings->app_id,
          gemini_enterprise_settings->location);
    }
    return nullptr;
  }

  bool ComputeCanAttach() const { return floating_panel_can_attach_; }

  void NotifyCanAttachChanged() {
    if (!web_client_) {
      return;
    }
    web_client_->NotifyPanelCanAttachChange(ComputeCanAttach());
  }

  void Uninstall() {
    host().SetInvocationSource(mojom::InvocationSource::kUnsupported);
    page_metadata_manager_.reset();
    SetAudioDucking(false, base::DoNothing());
    host().UnsetWebClient(this);
    pref_change_registrar_.Reset();
    local_state_pref_change_registrar_.Reset();
    host().RemovePanelStateObserver(this);
    focus_changed_subscription_ = {};
    pinned_tabs_changed_subscription_ = {};
    pinned_tab_data_changed_subscription_ = {};
    web_actuation_pref_subscription_ = {};
    consent_subscription_ = {};
    browser_attach_observation_.reset();
  }

  void WebClientDisconnected() {
    VLOG(1) << "Glic [WebClientHandler] WebClientDisconnected";
    Uninstall();
  }

  void OnUserEnabledActuationOnWebChanged() {
    web_client_->NotifyActuationOnWebSettingChanged(
        glic_service_->enabling().GetUserEnabledActuationOnWeb());
  }

  void OnConsentChanged() {
    web_client_->NotifyOnboardingCompletedChanged(
        glic_service_->enabling().GetCompletedFre() ==
        prefs::FreStatus::kCompleted);
  }

  void OnPrefChanged(const std::string& pref_name) {
    if (pref_name == prefs::kGlicMicrophoneEnabled) {
      web_client_->NotifyMicrophonePermissionStateChanged(
          pref_service_->GetBoolean(pref_name));
    } else if (pref_name == prefs::kGlicGeolocationEnabled) {
      web_client_->NotifyLocationPermissionStateChanged(
          pref_service_->GetBoolean(pref_name));
    } else if (pref_name == prefs::kGlicTabContextEnabled) {
      web_client_->NotifyTabContextPermissionStateChanged(
          pref_service_->GetBoolean(pref_name));
    } else if (pref_name == prefs::kGlicClosedCaptioningEnabled) {
      web_client_->NotifyClosedCaptioningSettingChanged(
          pref_service_->GetBoolean(pref_name));
    } else if (pref_name == prefs::kGlicDefaultTabContextEnabled) {
      web_client_->NotifyDefaultTabContextPermissionStateChanged(
          pref_service_->GetBoolean(pref_name));
    } else if (pref_name == glic::prefs::kGlicGeminiEnterpriseSettings) {
      web_client_->NotifyGeminiEnterpriseSettingsChanged(
          GetGeminiEnterpriseSettingsPtr());
    } else {
      DCHECK(false) << "Unknown Glic permission pref changed: " << pref_name;
    }
  }

  void OnLocalStatePrefChanged(const std::string& pref_name) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
    if (pref_name == prefs::kGlicLauncherHotkey) {
      web_client_->NotifyOsHotkeyStateChanged(GetHotkeyString());
    } else {
      CHECK(false) << "Unknown local state pref changed: " << pref_name;
    }
#endif
  }

  void OnFocusedTabChanged(const FocusedTabData& focused_tab_data) {
    MaybeNotifyFocusedTabChanged(CreateFocusedTabData(focused_tab_data));
  }

  void OnFocusedTabDataChanged(const glic::mojom::TabData* tab_data) {
    if (!tab_data) {
      return;
    }
    MaybeNotifyFocusedTabChanged(
        glic::mojom::FocusedTabData::NewFocusedTab(tab_data->Clone()));
  }

  void OnFocusedBrowserChanged(BrowserWindowInterface* browser_interface) {
    const bool is_browser_active = browser_interface != nullptr;
    NotifyInstanceActivationChanged(is_browser_active);
  }

  void NotifyInstanceActivationChanged(bool is_active) override {
    web_client_->NotifyInstanceActivationChanged(is_active);
  }


  void MaybeNotifyFocusedTabChanged(
      glic::mojom::FocusedTabDataPtr focused_tab_data) {
    if (debouncer_deduper_) {
      debouncer_deduper_->HandleUpdate(std::move(focused_tab_data));
      return;
    }
    NotifyWebClientFocusedTabChanged(std::move(focused_tab_data));
  }

  void NotifyWebClientFocusedTabChanged(glic::mojom::FocusedTabDataPtr data) {
    web_client_->NotifyFocusedTabChanged(std::move(data));
  }


  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  raw_ptr<Profile> profile_;
  raw_ptr<GlicPageHandler> page_handler_;
  raw_ptr<GlicKeyedService> glic_service_;
  raw_ptr<GlicInstanceCoordinator> window_controller_;
  raw_ptr<PrefService> pref_service_;
  ActiveStateCalculator active_state_calculator_;
  BrowserIsOpenCalculator browser_is_open_calculator_;
  base::CallbackListSubscription focus_changed_subscription_;
  base::CallbackListSubscription pinned_tabs_changed_subscription_;
  base::CallbackListSubscription pinned_tab_data_changed_subscription_;
  base::CallbackListSubscription tab_data_changed_subscription_;
  base::CallbackListSubscription focus_data_changed_subscription_;
  base::CallbackListSubscription focused_browser_changed_subscription_;
  base::CallbackListSubscription active_browser_changed_subscription_;
  base::CallbackListSubscription actor_task_state_changed_subscription_;
  base::CallbackListSubscription act_on_web_capability_changed_subscription_;
  base::CallbackListSubscription web_actuation_pref_subscription_;
  base::CallbackListSubscription consent_subscription_;
  mojo::Receiver<glic::mojom::WebClientHandler> receiver_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
  std::unique_ptr<BrowserAttachObservation> browser_attach_observation_;
  const std::unique_ptr<GlicAnnotationManager> annotation_manager_;
  std::unique_ptr<system_permission_settings::ScopedObservation>
      system_permission_settings_observation_;

  std::unique_ptr<DebouncerDeduper> debouncer_deduper_;
  std::unique_ptr<PageMetadataManager> page_metadata_manager_;
  bool floating_panel_can_attach_ = false;
};

GlicPageHandler::GlicPageHandler(
    content::WebContents* webui_contents,
    Host* host,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page)
    : host_(host),
      webui_contents_(webui_contents),
      browser_context_(webui_contents->GetBrowserContext()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  VLOG(1) << "Glic [PageHandler] Constructor";
  CHECK(host_);
  MarkProcessAsGlic(webui_contents->GetPrimaryMainFrame()->GetProcess());
  host_->WebUIPageHandlerAdded(this);
  host_->AddPanelStateObserver(this);
  UpdatePageState(host_->GetPanelState().kind);
  subscriptions_.push_back(
      GetGlicService()->enabling().RegisterProfileReadyStateChanged(
          base::BindRepeating(&GlicPageHandler::UpdateProfileReadyState,
                              base::Unretained(this))));
  UpdateProfileReadyState();
}

GlicPageHandler::~GlicPageHandler() {
  VLOG(1) << "Glic [PageHandler] Destructor";
  host_->RemovePanelStateObserver(this);
  WebUiStateChanged(glic::mojom::WebUiState::kUninitialized);
  // `GlicWebClientHandler` holds a pointer back to us, so delete it first.
  web_client_handler_.reset();
  // Clear `host_` before unregistering so the Host can be deleted
  // synchronously without leaving a dangling raw_ptr during teardown.
  Host* host = host_;
  host_ = nullptr;
  host->WebUIPageHandlerRemoved(this);
}

GlicKeyedService* GlicPageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_context_);
}

void GlicPageHandler::CreateWebClient(
    ::mojo::PendingReceiver<glic::mojom::WebClientHandler>
        web_client_receiver) {
  web_client_handler_ = std::make_unique<GlicWebClientHandler>(
      this, browser_context_, std::move(web_client_receiver));
}

void GlicPageHandler::PrepareForClient(
    base::OnceCallback<void(mojom::PrepareForClientResult)> callback) {
  TRACE_EVENT_INSTANT("glic", "GlicPageHandler::PrepareForClient - Request",
                      perfetto::Flow::FromPointer(this));

  auto wrapped_callback = base::BindOnce(
      [](base::WeakPtr<GlicPageHandler> origin_this,
         base::OnceCallback<void(mojom::PrepareForClientResult)> callback,
         mojom::PrepareForClientResult result) {
        if (origin_this) {
          TRACE_EVENT_INSTANT(
              "glic", "GlicPageHandler::PrepareForClient - Response",
              perfetto::TerminatingFlow::FromPointer(origin_this.get()));
        }
        std::move(callback).Run(std::move(result));
      },
      this->weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GetGlicService()->GetAuthController().CheckAuthBeforeLoad(
      std::move(wrapped_callback));
}

void GlicPageHandler::WebviewCommitted(const GURL& url) {
  VLOG(1) << "Glic [PageHandler] WebviewCommitted, url=" << url.spec();
  // TODO(crbug.com/388328847): Remove this code once launch issues are ironed
  // out.
  if (url.DomainIs("login.corp.google.com") ||
      url.DomainIs("accounts.google.com")) {
    host().LoginPageCommitted(this);
  }
}

void GlicPageHandler::OnZoomLevelChange(double zoom_factor) {
  // Ignore values outside of the supported range (defined in glic/webview.ts).
  if (zoom_factor < 1.0 || zoom_factor > 2.0) {
    LOG(ERROR) << "Glic [PageHandler] Invalid zoom level: " << zoom_factor;
    return;
  }
  int zoom_percent = std::round(zoom_factor * 100);
  auto* pref_service =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();
  // The webui sends a zoom level change on initialization. Skip these.
  if (pref_service->GetInteger(prefs::kGlicZoomLevel) != zoom_percent) {
    // Note that zoom level is already persisted in the glic webview partition -
    // this pref is only used for metrics.
    pref_service->SetInteger(prefs::kGlicZoomLevel, zoom_percent);
    host().instance_metrics().OnZoomLevelChange();
  }
}

void GlicPageHandler::NotifyWindowIntentToShow() {
  page_->IntentToShow();
}

void GlicPageHandler::Zoom(mojom::ZoomAction zoom_action) {
  auto* pref_service =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();
  int current_zoom = pref_service->GetInteger(prefs::kGlicZoomLevel);

  GlicZoomAction action_metric;
  switch (zoom_action) {
    case mojom::ZoomAction::kZoomIn:
      if (current_zoom >= 200) {
        action_metric = GlicZoomAction::kZoomInAtMax;
        base::RecordAction(base::UserMetricsAction("Glic.ZoomInAtMax"));
      } else {
        action_metric = GlicZoomAction::kZoomIn;
        base::RecordAction(base::UserMetricsAction("Glic.ZoomIn"));
      }
      break;
    case mojom::ZoomAction::kZoomOut:
      if (current_zoom <= 100) {
        action_metric = GlicZoomAction::kZoomOutAtMin;
        base::RecordAction(base::UserMetricsAction("Glic.ZoomOutAtMin"));
      } else {
        action_metric = GlicZoomAction::kZoomOut;
        base::RecordAction(base::UserMetricsAction("Glic.ZoomOut"));
      }
      break;
    case mojom::ZoomAction::kReset:
      action_metric = GlicZoomAction::kReset;
      base::RecordAction(base::UserMetricsAction("Glic.ZoomReset"));
      break;
  }

  base::UmaHistogramEnumeration("Glic.ZoomAction", action_metric);
  page_->Zoom(zoom_action);
}

content::RenderFrameHost* GlicPageHandler::GetGuestMainFrame() {
  // Note: Eventually Glic will fully migrate to SlimWebView.
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::WebViewGuest* web_view_guest = nullptr;
  content::RenderFrameHost* webui_frame =
      webui_contents_->GetPrimaryMainFrame();
  if (!webui_frame) {
    return nullptr;
  }
  webui_frame->ForEachRenderFrameHostWithAction(
      [&web_view_guest](content::RenderFrameHost* rfh) {
        auto* web_view = extensions::WebViewGuest::FromRenderFrameHost(rfh);
        if (web_view && web_view->attached()) {
          web_view_guest = web_view;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return web_view_guest ? web_view_guest->GetGuestMainFrame() : nullptr;
#else
  guest_view::SlimWebViewGuest* slim_web_view_guest = nullptr;
  content::RenderFrameHost* webui_frame =
      webui_contents_->GetPrimaryMainFrame();
  if (!webui_frame) {
    return nullptr;
  }
  webui_frame->ForEachRenderFrameHostWithAction(
      [&slim_web_view_guest](content::RenderFrameHost* rfh) {
        auto* web_view = guest_view::SlimWebViewGuest::FromRenderFrameHost(rfh);
        if (web_view && web_view->attached()) {
          slim_web_view_guest = web_view;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return slim_web_view_guest ? slim_web_view_guest->GetGuestMainFrame()
                             : nullptr;
#endif
}

void GlicPageHandler::SetProfileReadyState(
    glic::mojom::ProfileReadyState ready_state) {
  page_->SetProfileReadyState(ready_state);
}

void GlicPageHandler::ClosePanel(ClosePanelCallback callback) {
  host().ClosePanel(this);
  std::move(callback).Run();
}

void GlicPageHandler::OpenProfilePickerAndClosePanel() {
  glic::GlicProfileManager::GetInstance()->ShowProfilePicker();
  host().ClosePanel(this);
}

void GlicPageHandler::OpenDisabledByAdminLinkAndClosePanel() {
  GURL disabled_by_admin_link_url = GURL(features::kGlicCaaLinkUrl.Get());
  std::unique_ptr<NavigateParams> params = std::make_unique<NavigateParams>(
      Profile::FromBrowserContext(browser_context_), disabled_by_admin_link_url,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  glic::NavigateAsync(std::move(params), base::DoNothing());
  host().ClosePanel(this);
  base::RecordAction(
      base::UserMetricsAction("Glic.DisabledByAdminPanelLinkClicked"));
}

void GlicPageHandler::OpenHelpCenterTopicAndClosePanel(
    glic::mojom::HelpCenterTopic topic) {
  // Safe fallback URL in case a newer web client passes a topic not yet
  // supported by this version of Chrome.
  GURL help_url = GURL(features::kGlicIneligibleAccountHelpUrl.Get());
  switch (topic) {
    case mojom::HelpCenterTopic::kLocationMismatch:
      help_url = GURL(features::kGlicLocationMismatchHelpUrl.Get());
      break;
    case mojom::HelpCenterTopic::kIneligibleAccount:
      help_url = GURL(features::kGlicIneligibleAccountHelpUrl.Get());
      break;
  }
  auto params = std::make_unique<NavigateParams>(
      Profile::FromBrowserContext(browser_context_), help_url,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  glic::Navigate(std::move(params));
  host().ClosePanel(this);
}

void GlicPageHandler::SignInAndClosePanel() {
  GetGlicService()->GetAuthController().ShowReauthForAccount(webui_contents_);
}

void GlicPageHandler::ResizeWidget(const gfx::Size& size,
                                   base::TimeDelta duration,
                                   ResizeWidgetCallback callback) {
  host().ResizePanel(this, size, duration, std::move(callback));
}

void GlicPageHandler::EnableDragResize(bool enabled) {
  // features::kGlicUserResize is not checked here because the WebUI page
  // invokes this method when it is disabled, too (when its state changes).
  host().EnableDragResize(this, enabled);
}

void GlicPageHandler::WebUiStateChanged(glic::mojom::WebUiState new_state) {
  host().WebUiStateChanged(this, new_state);
}

void GlicPageHandler::PanelStateChanged(
    const glic::mojom::PanelState& panel_state) {
  UpdatePageState(panel_state.kind);
}

void GlicPageHandler::UpdatePageState(mojom::PanelStateKind panelStateKind) {
  page_->UpdatePageState(panelStateKind);
}

void GlicPageHandler::UpdateProfileReadyState() {
  page_->SetProfileReadyState(GlicEnabling::GetProfileReadyState(
      Profile::FromBrowserContext(browser_context_)));
}

void GlicPageHandler::ZeroStateSuggestionChanged(
    mojom::ZeroStateSuggestionsV2Ptr returned_suggestions,
    mojom::ZeroStateSuggestionsOptions returned_options) {
  if (!web_client_handler_) {
    return;
  }

  auto options = mojom::ZeroStateSuggestionsOptions::New();
  options->is_first_run = std::move(returned_options.is_first_run);
  options->supported_tools = std::move(returned_options.supported_tools);
  web_client_handler_->NotifyZeroStateSuggestionsChanged(
      std::move(returned_suggestions), std::move(options));
}

}  // namespace glic
