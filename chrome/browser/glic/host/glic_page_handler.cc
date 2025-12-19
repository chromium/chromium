// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_page_handler.h"

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/aggregated_journal_file_serializer.h"
#include "chrome/browser/actor/aggregated_journal_in_memory_serializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom-data-view.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_annotation_manager.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/glic_synthetic_trial_manager.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/host/page_metadata_manager.h"
#include "chrome/browser/glic/media/glic_media_link_helper.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/feedback/content/content_tracing_manager.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/metrics/metrics_service.h"
#include "components/optimization_guide/content/browser/page_content_metadata_observer.h"
#include "components/optimization_guide/core/model_quality/model_quality_util.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "pdf/buildflags.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

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

#if BUILDFLAG(IS_MAC)
constexpr mojom::Platform kPlatform = mojom::Platform::kMacOS;
#elif BUILDFLAG(IS_WIN)
constexpr mojom::Platform kPlatform = mojom::Platform::kWindows;
#elif BUILDFLAG(IS_LINUX)
constexpr mojom::Platform kPlatform = mojom::Platform::kLinux;
#elif BUILDFLAG(IS_CHROMEOS)
constexpr mojom::Platform kPlatform = mojom::Platform::kChromeOS;
#else
constexpr mojom::Platform kPlatform = mojom::Platform::kUnknown;
#endif

mojom::GetContextResultPtr LogErrorAndUnwrapResult(
    base::OnceCallback<void(GlicGetContextFromTabError)> error_logger,
    GlicGetContextResult result) {
  if (!result.has_value()) {
    std::move(error_logger).Run(result.error().error_code);
    return mojom::GetContextResult::NewErrorReason(result.error().message);
  }
  return std::move(result.value());
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
    PanelStateChanged(host_->GetPanelState(nullptr),
                      {.attached_browser = nullptr, .glic_widget = nullptr});
  }
  ~ActiveStateCalculator() override { host_->RemovePanelStateObserver(this); }

  bool IsActive() const { return is_active_; }
  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }
  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // GlicWindowController::StateObserver implementation.
  void PanelStateChanged(const glic::mojom::PanelState& panel_state,
                         const PanelStateContext& context) override {
    panel_state_kind_ = panel_state.kind;
    SetAttachedBrowser(context.attached_browser);
    PostRecalcAndNotify();
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

  void AttachedBrowserActiveChanged(BrowserWindowInterface* browser) {
    PostRecalcAndNotify();
  }

  void AttachedBrowserDidClose(BrowserWindowInterface* browser) {
    SetAttachedBrowser(nullptr);
    PostRecalcAndNotify();
  }

  bool SetAttachedBrowser(BrowserWindowInterface* attached_browser) {
    if (attached_browser_ == attached_browser) {
      return false;
    }
    attached_browser_subscriptions_.clear();
    attached_browser_ = attached_browser;

    if (attached_browser_ && !attached_browser_->GetBrowserForMigrationOnly()
                                  ->is_delete_scheduled()) {
      attached_browser_subscriptions_.push_back(
          attached_browser_->RegisterDidBecomeActive(base::BindRepeating(
              &ActiveStateCalculator::AttachedBrowserActiveChanged,
              base::Unretained(this))));
      attached_browser_subscriptions_.push_back(
          attached_browser_->RegisterDidBecomeInactive(base::BindRepeating(
              &ActiveStateCalculator::AttachedBrowserActiveChanged,
              base::Unretained(this))));
      attached_browser_subscriptions_.push_back(
          attached_browser_->RegisterBrowserDidClose(base::BindRepeating(
              &ActiveStateCalculator::AttachedBrowserDidClose,
              base::Unretained(this))));
    }
    return true;
  }

  bool Calculate() {
    if (panel_state_kind_ == glic::mojom::PanelStateKind::kHidden) {
      return false;
    }
    // TODO(b:444463509): Implement better calculation.
    if (GlicEnabling::IsMultiInstanceEnabled()) {
      return true;
    }
    if (!attached_browser_) {
      return true;
    }
    if (attached_browser_->GetBrowserForMigrationOnly()
            ->is_delete_scheduled()) {
      return false;
    }

    return attached_browser_->IsActive();
  }

  base::OneShotTimer calc_timer_;
  std::vector<base::CallbackListSubscription> attached_browser_subscriptions_;

  raw_ptr<Host> host_;
  base::ObserverList<Observer> observers_;
  glic::mojom::PanelStateKind panel_state_kind_;
  bool is_active_ = false;
  raw_ptr<BrowserWindowInterface> attached_browser_ = nullptr;
};

class BrowserIsOpenCalculator : public BrowserListObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void BrowserIsOpenChanged(bool browser_is_open) = 0;
  };

  explicit BrowserIsOpenCalculator(Profile* profile, Observer* observer)
      : profile_(profile) {
    BrowserList::AddObserver(this);
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [this](BrowserWindowInterface* browser_window_interface) {
          OnBrowserAdded(
              browser_window_interface->GetBrowserForMigrationOnly());
          return true;
        });
    // Don't notify observer during construction.
    observer_ = observer;
  }
  ~BrowserIsOpenCalculator() override { BrowserList::RemoveObserver(this); }

  void OnBrowserAdded(Browser* browser) override {
    if (browser->profile() == profile_) {
      UpdateBrowserCount(1);
    }
  }
  void OnBrowserRemoved(Browser* browser) override {
    if (browser->profile() == profile_) {
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
  // Profile outlives this class. The glic web contents is torn down along with
  // GlicKeyedService, which is tied to the profile.
  raw_ptr<Profile> profile_;
  raw_ptr<Observer> observer_ = nullptr;
  int open_browser_count_ = 0;
};

// Does time-based debouncing and cache-based deduping of FocusedTabData
// updates.
// TODO(b/424242331): Debouncing & deduping should happen closer to where
// focused tab updates are generated.
// TODO(b/424242331): This logic should be moved to a separate file and be made
// more generic and configurable.
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

const char kGlicActorJournalLog[] = "glic-actor-journal";

// Class that encapsulates interacting with the actor journal.
class JournalHandler {
 public:
  explicit JournalHandler(Profile* profile)
      : actor_keyed_service_(actor::ActorKeyedService::Get(profile)) {
    base::FilePath path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            kGlicActorJournalLog);
    if (!path.empty()) {
      path = base::GetUniquePathWithSuffixFormat(path, "_%d");
      LOG(ERROR) << "Glic Journal: " << path;
      file_journal_serializer_ =
          std::make_unique<actor::AggregatedJournalFileSerializer>(
              actor_keyed_service_->GetJournal());
      file_journal_serializer_->Init(
          path, base::BindOnce(&JournalHandler::FileInitDone,
                               base::Unretained(this)));
    }
  }

  void LogBeginAsyncEvent(uint64_t event_async_id,
                          int32_t task_id,
                          const std::string& event,
                          const std::string& details) {
    // If there is a matching ID make sure it terminates before the new event is
    // created.
    auto it = active_journal_events_.find(event_async_id);
    if (it != active_journal_events_.end()) {
      active_journal_events_.erase(it);
    }

    auto actor_task_id = actor::TaskId(task_id);
    active_journal_events_[event_async_id] =
        actor_keyed_service_->GetJournal().CreatePendingAsyncEntry(
            /*url=*/GURL::EmptyGURL(), actor_task_id,
            actor::MakeFrontEndTrackUUID(actor_task_id), event,
            actor::JournalDetailsBuilder()
                .Add("begin_details", details)
                .Build());
  }

  void LogEndAsyncEvent(mojom::WebClientModel model,
                        uint64_t event_async_id,
                        const std::string& details) {
    auto it = active_journal_events_.find(event_async_id);
    if (it != active_journal_events_.end()) {
      it->second->EndEntry(
          actor::JournalDetailsBuilder().Add("end_details", details).Build());

      if (model == mojom::WebClientModel::kActor) {
        // Log a histogram for each async event.
        std::string histogram_name;
        // The event name may have whitespaces and that won't work as a
        // histogram name.
        base::RemoveChars(it->second->event_name(), " ", &histogram_name);

        base::UmaHistogramLongTimes100(
            "Glic.Actor.JournalEvent." + histogram_name,
            base::TimeTicks::Now() - it->second->begin_time());
      }

      active_journal_events_.erase(it);
    }
  }

  void LogInstantEvent(int32_t task_id,
                       const std::string& event,
                       const std::string& details) {
    auto actor_task_id = actor::TaskId(task_id);
    actor_keyed_service_->GetJournal().Log(
        /*url=*/GURL::EmptyGURL(), actor_task_id,
        actor::MakeFrontEndTrackUUID(actor_task_id), event,
        actor::JournalDetailsBuilder().Add("details", details).Build());
  }

  void Clear() {
    if (journal_serializer_) {
      journal_serializer_->Clear();
    }
  }

  void Snapshot(
      bool clear_journal,
      glic::mojom::WebClientHandler::JournalSnapshotCallback callback) {
    if (!journal_serializer_) {
      std::move(callback).Run(glic::mojom::Journal::New());
      return;
    }
    std::move(callback).Run(
        glic::mojom::Journal::New(journal_serializer_->Snapshot()));
    if (clear_journal) {
      journal_serializer_->Clear();
    }
  }

  std::vector<uint8_t> GetSnapshot(bool clear_journal) {
    std::vector<uint8_t> result_buffer;
    if (journal_serializer_) {
      result_buffer = journal_serializer_->Snapshot();
      if (clear_journal) {
        journal_serializer_->Clear();
      }
    }
    return result_buffer;
  }

  void Start(uint64_t max_bytes, bool capture_screenshots) {
    journal_serializer_ =
        std::make_unique<actor::AggregatedJournalInMemorySerializer>(
            actor_keyed_service_->GetJournal(), max_bytes);
    journal_serializer_->Init();
  }

  void Stop() { journal_serializer_.reset(); }

  void RecordFeedback(bool positive, const std::string& reason) {
    if (base::FeatureList::IsEnabled(features::kGlicRecordActorJournal) &&
        !positive) {
      SendResponseFeedback(reason);
    }
  }

 private:
  void SendResponseFeedback(const std::string& reason) {
    base::WeakPtr<feedback::FeedbackUploader> uploader =
        feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(
            actor_keyed_service_->GetProfile())
            ->AsWeakPtr();
    scoped_refptr<::feedback::FeedbackData> feedback_data =
        base::MakeRefCounted<feedback::FeedbackData>(
            std::move(uploader), ContentTracingManager::Get());
    auto journal = GetSnapshot(false);

    // TODO(b/430054430): Fetch and include system data to the feedback.
    feedback_data->set_description(
        reason + " - " + base::Uuid::GenerateRandomV4().AsLowercaseString());
    feedback_data->set_product_id(
        features::kGlicRecordActorJournalFeedbackProductId.Get());
    feedback_data->set_category_tag(
        features::kGlicRecordActorJournalFeedbackCategoryTag.Get());
    feedback_data->set_is_offensive_or_unsafe(false);
    feedback_data->AddFile("actor-journal", journal);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(
            actor_keyed_service_->GetProfile());
    if (identity_manager &&
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      feedback_data->set_user_email(
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .email);
    }

    system_logs::BuildChromeSystemLogsFetcher(
        actor_keyed_service_->GetProfile(), /*scrub_data=*/false)
        ->Fetch(base::BindOnce(
            [](scoped_refptr<::feedback::FeedbackData> feedback_data,
               std::unique_ptr<system_logs::SystemLogsResponse>
                   system_logs_response) {
              if (system_logs_response) {
                feedback_data->AddLogs(*system_logs_response);
              }
              feedback_data->CompressSystemInfo();
              feedback_data->OnFeedbackPageDataComplete();
            },
            std::move(feedback_data)));
  }

  void FileInitDone(bool success) {
    if (!success) {
      file_journal_serializer_.reset();
    }
  }

  absl::flat_hash_map<
      uint64_t,
      std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>>
      active_journal_events_;
  std::unique_ptr<actor::AggregatedJournalInMemorySerializer>
      journal_serializer_;
  std::unique_ptr<actor::AggregatedJournalFileSerializer>
      file_journal_serializer_;
  raw_ptr<actor::ActorKeyedService> actor_keyed_service_;
};

}  // namespace

// WARNING: One instance of this class is created per WebUI navigated to
// chrome://glic. The design and implementation of this class, which plumbs
// events through GlicKeyedService to other components, relies on the assumption
// that there is exactly 1 WebUI instance. If this assumption is ever violated
// then many classes will break.
//
// TODO(crbug.com/458761731): Once `loadAndExtractContent` is defined in the
// handler mojom interface, override and implement its mojom declaration.
class GlicWebClientHandler : public glic::mojom::WebClientHandler,
                             public GlicWindowController::StateObserver,
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
        window_controller_(&glic_service_->window_controller()),
        pref_service_(profile_->GetPrefs()),
        active_state_calculator_(&page_handler_->host()),
        browser_is_open_calculator_(profile_, this),
        receiver_(this, std::move(receiver)),
        annotation_manager_(
            std::make_unique<GlicAnnotationManager>(glic_service_)),
        journal_handler_(profile_) {
    active_state_calculator_.AddObserver(this);
  }

  ~GlicWebClientHandler() override {
    active_state_calculator_.RemoveObserver(this);
    if (web_client_) {
      Uninstall();
    }
  }

  Host& host() { return page_handler_->host(); }
  GlicSharingManager& sharing_manager() { return host().sharing_manager(); }

  // glic::mojom::WebClientHandler implementation.
  void SwitchConversation(glic::mojom::ConversationInfoPtr info,
                          SwitchConversationCallback callback) override {
    if (info && info->conversation_id.empty()) {
      receiver_.ReportBadMessage("conversation_id cannot be empty.");
    }
    page_handler_->host().SwitchConversation(std::move(info),
                                             std::move(callback));
  }

  void RegisterConversation(glic::mojom::ConversationInfoPtr info,
                            RegisterConversationCallback callback) override {
    if (info->conversation_id.empty()) {
      receiver_.ReportBadMessage("conversation_id cannot be empty.");
    }
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

    NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.opened_by_another_window = true;
    params.window_features.bounds = gfx::Rect(x, y, popup_width, popup_height);
    Navigate(&params);
  }

  void WebClientCreated(
      ::mojo::PendingRemote<glic::mojom::WebClient> web_client,
      WebClientCreatedCallback callback) override {
    web_client_.Bind(std::move(web_client));
    web_client_.set_disconnect_handler(base::BindOnce(
        &GlicWebClientHandler::WebClientDisconnected, base::Unretained(this)));

    page_metadata_manager_ =
        std::make_unique<PageMetadataManager>(web_client_.get());

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
        prefs::kGlicUserEnabledActuationOnWeb,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
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
        sharing_manager().AddFocusedTabChangedCallback(
            base::BindRepeating(&GlicWebClientHandler::OnFocusedTabChanged,
                                base::Unretained(this)));

    pinned_tabs_changed_subscription_ =
        sharing_manager().AddPinnedTabsChangedCallback(base::BindRepeating(
            &GlicWebClientHandler::OnPinningChanged, base::Unretained(this)));

    pinned_tab_data_changed_subscription_ =
        sharing_manager().AddPinnedTabDataChangedCallback(
            base::BindRepeating(&GlicWebClientHandler::OnPinnedTabDataChanged,
                                base::Unretained(this)));

    if (base::FeatureList::IsEnabled(features::kGlicGetTabByIdApi)) {
      tab_data_changed_subscription_ =
          glic_service_->AddTabDataChangedCallback(base::BindRepeating(
              &GlicWebClientHandler::OnTabDataChanged, base::Unretained(this)));
    }

    focus_data_changed_subscription_ =
        sharing_manager().AddFocusedTabDataChangedCallback(
            base::BindRepeating(&GlicWebClientHandler::OnFocusedTabDataChanged,
                                base::Unretained(this)));

    if (!GlicEnabling::IsMultiInstanceEnabled()) {
      focused_browser_changed_subscription_ =
          sharing_manager().AddFocusedBrowserChangedCallback(
              base::BindRepeating(
                  &GlicWebClientHandler::OnFocusedBrowserChanged,
                  base::Unretained(this)));
    }

    if (!GlicEnabling::IsMultiInstanceEnabled()) {
      browser_attach_observation_ = ObserveBrowserForAttachment(profile_, this);
    }

    system_permission_settings_observation_ =
        system_permission_settings::Observe(base::BindRepeating(
            &GlicWebClientHandler::OnOsPermissionSettingChanged,
            base::Unretained(this)));

    if (base::FeatureList::IsEnabled(features::kGlicActor)) {
      if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
        actor_task_state_changed_subscription_ =
            actor_service->AddTaskStateChangedCallback(base::BindRepeating(
                &GlicWebClientHandler::NotifyActorTaskStateChanged,
                base::Unretained(this)));
        // CallbackListSubscription prevents these callbacks from being invoked
        // when this object is destructed.
        // TODO(crbug.com/445224605): Right now this code assumes that
        //   ActorKeyedService only owns a single Execution engine instance.
        act_on_web_capability_changed_subscription_ =
            actor_service->AddActOnWebCapabilityChangedCallback(
                base::BindRepeating(
                    &GlicWebClientHandler::NotifyActOnWebCapabilityChanged,
                    base::Unretained(this)));
      }
    }

    auto state = glic::mojom::WebClientInitialState::New();
    state->chrome_version = version_info::GetVersion();
    state->platform = kPlatform;
    state->microphone_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicMicrophoneEnabled);
    state->location_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicGeolocationEnabled);
    state->tab_context_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicTabContextEnabled);
    state->os_location_permission_enabled =
        system_permission_settings::IsAllowed(ContentSettingsType::GEOLOCATION);

    state->panel_state = host().GetPanelState(this).Clone();

    state->focused_tab_data =
        CreateFocusedTabData(sharing_manager().GetFocusedTabData());
    state->can_attach = ComputeCanAttach();
    state->panel_is_active = active_state_calculator_.IsActive();

    if (base::FeatureList::IsEnabled(glic::mojom::features::kGlicMultiTab)) {
      OnPinningChanged(sharing_manager().GetPinnedTabs());
    }

    state->browser_is_open = browser_is_open_calculator_.IsOpen();
    state->instance_is_active = host().instance_delegate().IsActive();

    state->always_detached_mode = GlicWindowController::AlwaysDetached();

    state->enable_act_in_focused_tab =
        base::FeatureList::IsEnabled(features::kGlicActor);
    state->enable_scroll_to =
        base::FeatureList::IsEnabled(features::kGlicScrollTo);
    state->enable_zero_state_suggestions =
        contextual_cueing::IsZeroStateSuggestionsEnabled();

    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
    local_state_pref_change_registrar_.Add(
        prefs::kGlicLauncherHotkey,
        base::BindRepeating(&GlicWebClientHandler::OnLocalStatePrefChanged,
                            base::Unretained(this)));
    state->hotkey = GetHotkeyString();
    state->enable_default_tab_context_setting_feature =
        base::FeatureList::IsEnabled(features::kGlicDefaultTabContextSetting);
    state->default_tab_context_setting_enabled =
        pref_service_->GetBoolean(prefs::kGlicDefaultTabContextEnabled);
    state->enable_closed_captioning_feature =
        base::FeatureList::IsEnabled(features::kGlicClosedCaptioning);
    state->closed_captioning_setting_enabled =
        pref_service_->GetBoolean(prefs::kGlicClosedCaptioningEnabled);
    state->enable_maybe_refresh_user_status =
        base::FeatureList::IsEnabled(features::kGlicUserStatusCheck) &&
        features::kGlicUserStatusRefreshApi.Get();
    state->enable_multi_tab =
        base::FeatureList::IsEnabled(glic::mojom::features::kGlicMultiTab);
    state->enable_get_context_actor = base::FeatureList::IsEnabled(
        glic::mojom::features::kGlicActorTabContext);
    state->enable_web_actuation_setting_feature =
        base::FeatureList::IsEnabled(features::kGlicWebActuationSetting);
    state->actuation_on_web_setting_enabled =
        pref_service_->GetBoolean(prefs::kGlicUserEnabledActuationOnWeb);

#if BUILDFLAG(ENABLE_PDF)
    if (features::kGlicScrollToPDF.Get()) {
      state->host_capabilities.push_back(mojom::HostCapability::kScrollToPdf);
    }
#endif
    if (base::FeatureList::IsEnabled(
            features::kGlicPanelResetSizeAndLocationOnOpen)) {
      state->host_capabilities.push_back(
          mojom::HostCapability::kResetSizeAndLocationOnOpen);
    }
    if (GlicEnabling::IsMultiInstanceEnabled()) {
      state->host_capabilities.push_back(mojom::HostCapability::kMultiInstance);
    }
    state->enable_get_page_metadata =
        base::FeatureList::IsEnabled(blink::features::kFrameMetadataObserver);
    state->enable_api_activation_gating =
        base::FeatureList::IsEnabled(features::kGlicApiActivationGating);
    if (base::FeatureList::IsEnabled(
            glic::mojom::features::kGlicAppendModelQualityClientId)) {
      state->host_capabilities.push_back(
          mojom::HostCapability::kGetModelQualityClientId);
    }
    state->enable_capture_region =
        base::FeatureList::IsEnabled(features::kGlicCaptureRegion);
    state->can_act_on_web = false;
    if (base::FeatureList::IsEnabled(features::kGlicActor)) {
      if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
        state->can_act_on_web =
            actor_service->GetPolicyChecker().can_act_on_web();
      }
    }
    state->enable_activate_tab = base::FeatureList::IsEnabled(
        glic::mojom::features::kGlicActivateTabApi);
    state->enable_get_tab_by_id =
        base::FeatureList::IsEnabled(features::kGlicGetTabByIdApi);
    state->enable_open_password_manager_settings_page =
        base::FeatureList::IsEnabled(
            features::kGlicOpenPasswordManagerSettingsPageApi);

    std::move(callback).Run(std::move(state));
  }

  void WebClientInitializeFailed() override {
    host().WebClientInitializeFailed(this);
  }

  void WebClientInitialized() override {
    host().SetWebClient(this);
    // If chrome://glic is opened in a tab for testing, send a synthetic open
    // signal.
    if (page_handler_->webui_contents() != host().webui_contents()) {
      mojom::PanelOpeningDataPtr panel_opening_data =
          mojom::PanelOpeningData::New();
      panel_opening_data->panel_state = host().GetPanelState(this).Clone();
      panel_opening_data->invocation_source =
          mojom::InvocationSource::kUnsupported;
      base::UmaHistogramBoolean("Glic.Host.OpenedInRegularTab", true);
      web_client_->NotifyPanelWillOpen(std::move(panel_opening_data),
                                       base::DoNothing());
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
                 bool open_in_background,
                 const std::optional<int32_t> window_id,
                 CreateTabCallback callback) override {
    if (base::FeatureList::IsEnabled(media::kMediaLinkHelpers)) {
      if (auto* tab = sharing_manager().GetFocusedTabData().focus()) {
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

  void OpenGlicSettingsPage(mojom::OpenSettingsOptionsPtr options) override {
    switch (options->highlightField) {
      case mojom::SettingsPageField::kOsHotkey:
        ::glic::OpenGlicKeyboardShortcutSetting(profile_);
        base::RecordAction(
            base::UserMetricsAction("GlicSessionSettingsOpened.OsHotkey"));
        break;
      case mojom::SettingsPageField::kOsEntrypointToggle:
        ::glic::OpenGlicOsToggleSetting(profile_);
        base::RecordAction(base::UserMetricsAction(
            "GlicSessionSettingsOpened.OsEntrypointToggle"));
        break;
      case mojom::SettingsPageField::kNone:  // Default value.
        ::glic::OpenGlicSettingsPage(profile_);
        base::RecordAction(
            base::UserMetricsAction("GlicSessionSettingsOpened.Default"));
        break;
    }
  }

  void OpenPasswordManagerSettingsPage() override {
    if (!base::FeatureList::IsEnabled(
            features::kGlicOpenPasswordManagerSettingsPageApi)) {
      return;
    }
    ::glic::OpenPasswordManagerSettingsPage(profile_);
  }

  void ClosePanel() override { host().ClosePanel(page_handler_); }

  void ClosePanelAndShutdown() override {
    if (GlicEnabling::IsMultiInstanceEnabled()) {
      ClosePanel();
    } else {
      // This call will tear down the web client after closing the window.
      glic_service_->CloseAndShutdown();
    }
  }

  void AttachPanel() override {
    if (GlicWindowController::AlwaysDetached()) {
      receiver_.ReportBadMessage(
          "AttachPanel cannot be called when always detached mode is enabled.");
      return;
    }
    host().AttachPanel(page_handler_);
  }

  void DetachPanel() override {
    if (GlicWindowController::AlwaysDetached()) {
      receiver_.ReportBadMessage(
          "DetachPanel cannot be called when always detached mode is enabled.");
      return;
    }
    host().DetachPanel(page_handler_);
  }

  void ShowProfilePicker() override {
    glic::GlicProfileManager::GetInstance()->ShowProfilePicker();
  }

  void OnModeChange(glic::mojom::WebClientMode new_mode) override {
    glic_service_->metrics()->SetWebClientMode(new_mode);
    host().OnInteractionModeChange(page_handler_, new_mode);
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
      glic::mojom::GetTabContextOptionsPtr options,
      GetContextFromFocusedTabCallback callback) override {
    FocusedTabData ftd = sharing_manager().GetFocusedTabData();
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
    if (auto* instance_metrics = host().instance_metrics()) {
      instance_metrics->DidRequestContextFromFocusedTab();
    }
    auto tab_handle = tab ? tab->GetHandle() : tabs::TabHandle::Null();
    sharing_manager().GetContextFromTab(
        tab_handle, *options,
        base::BindOnce(
            &LogErrorAndUnwrapResult,
            base::BindOnce(&GlicMetrics::LogGetContextFromFocusedTabError,
                           base::Unretained(glic_service_->metrics())))
            .Then(std::move(callback)));
  }

  void GetContextFromTab(int32_t tab_id,
                         glic::mojom::GetTabContextOptionsPtr options,
                         GetContextFromTabCallback callback) override {
    // Extra activation gating is done in this function.
    sharing_manager().GetContextFromTab(
        tabs::TabHandle(tab_id), *options,
        base::BindOnce(
            &LogErrorAndUnwrapResult,
            base::BindOnce(&GlicMetrics::LogGetContextFromTabError,
                           base::Unretained(glic_service_->metrics())))
            .Then(std::move(callback)));
  }

  void GetContextForActorFromTab(
      int32_t tab_id,
      glic::mojom::GetTabContextOptionsPtr options,
      GetContextForActorFromTabCallback callback) override {
    sharing_manager().GetContextForActorFromTab(
        tabs::TabHandle(tab_id), *options,
        base::BindOnce(
            &LogErrorAndUnwrapResult,
            base::BindOnce(&GlicMetrics::LogGetContextForActorFromTabError,
                           base::Unretained(glic_service_->metrics())))
            .Then(std::move(callback)));
  }

  void SetMaximumNumberOfPinnedTabs(
      uint32_t num_tabs,
      SetMaximumNumberOfPinnedTabsCallback callback) override {
    uint32_t effective_max = sharing_manager().SetMaxPinnedTabs(num_tabs);
    std::move(callback).Run(effective_max);
  }

  void PinTabs(const std::vector<int32_t>& tab_ids,
               PinTabsCallback callback) override {
    std::vector<tabs::TabHandle> tab_handles;
    for (auto tab_id : tab_ids) {
      tab_handles.push_back(tabs::TabHandle(tab_id));
    }
    std::move(callback).Run(sharing_manager().PinTabs(
        tab_handles, GlicPinTrigger::kWebClientUnknown));
  }

  void UnpinTabs(const std::vector<int32_t>& tab_ids,
                 UnpinTabsCallback callback) override {
    std::vector<tabs::TabHandle> tab_handles;
    for (auto tab_id : tab_ids) {
      tab_handles.push_back(tabs::TabHandle(tab_id));
    }
    std::move(callback).Run(sharing_manager().UnpinTabs(
        tab_handles, GlicUnpinTrigger::kWebClientUnknown));
  }

  void UnpinAllTabs() override {
    sharing_manager().UnpinAllTabs(GlicUnpinTrigger::kWebClientUnknown);
  }

  void CreateTask(actor::webui::mojom::TaskOptionsPtr options,
                  CreateTaskCallback callback) override {
    host().instance_delegate().CreateTask(nullptr, std::move(options),
                                          std::move(callback));
  }

  void PerformActions(const std::vector<uint8_t>& actions_proto,
                      PerformActionsCallback callback) override {
    host().instance_delegate().PerformActions(actions_proto,
                                              std::move(callback));
  }

  void StopActorTask(int32_t task_id,
                     mojom::ActorTaskStopReason stop_reason) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "StopActorTask cannot be called without GlicActor enabled.");
      return;
    }
    host().instance_delegate().StopActorTask(actor::TaskId(task_id),
                                             stop_reason);
  }

  void PauseActorTask(int32_t task_id,
                      mojom::ActorTaskPauseReason pause_reason,
                      std::optional<int32_t> tab_id) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "PauseActorTask cannot be called without GlicActor enabled.");
      return;
    }
    tabs::TabInterface::Handle tab_handle;
    if (tab_id.has_value()) {
      tab_handle = tabs::TabInterface::Handle(*tab_id);
    }
    host().instance_delegate().PauseActorTask(actor::TaskId(task_id),
                                              pause_reason, tab_handle);
  }

  void ResumeActorTask(int32_t task_id,
                       glic::mojom::GetTabContextOptionsPtr context_options,
                       ResumeActorTaskCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "ResumeActorTask cannot be called without GlicActor enabled.");
      return;
    }
    host().instance_delegate().ResumeActorTask(
        actor::TaskId(task_id), *context_options, std::move(callback));
  }

  void InterruptActorTask(int32_t task_id) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "InterruptActorTask cannot be called without GlicActor enabled.");
      return;
    }
    host().instance_delegate().InterruptActorTask(actor::TaskId(task_id));
  }

  void UninterruptActorTask(int32_t task_id) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "UninterruptActorTask cannot be called without GlicActor enabled.");
      return;
    }
    host().instance_delegate().UninterruptActorTask(actor::TaskId(task_id));
  }

  void CreateActorTab(int32_t task_id,
                      bool open_in_background,
                      std::optional<int32_t> initiator_tab_id,
                      std::optional<int32_t> initiator_window_id,
                      CreateActorTabCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "StopActorTask cannot be called without GlicActor enabled.");
      return;
    }
    host().instance_delegate().CreateActorTab(
        actor::TaskId(task_id), open_in_background, initiator_tab_id,
        initiator_window_id, std::move(callback));
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

  void CaptureRegion(
      mojo::PendingRemote<mojom::CaptureRegionObserver> observer) override {
    content::WebContents* web_contents = nullptr;
    const FocusedTabData& focus = sharing_manager().GetFocusedTabData();
    // Prioritize the focused tab, but fall back to the unfocused tab if one is
    // available. This is useful in cases where the active tab is not
    // "focusable" by Glic (e.g. chrome:// pages).
    tabs::TabInterface* active_tab =
        focus.is_focus() ? focus.focus() : focus.unfocused_tab();

    if (active_tab) {
      web_contents = active_tab->GetContents();
    }
    glic_service_->CaptureRegion(web_contents, std::move(observer));
  }

  void SetAudioDucking(bool enabled,
                       SetAudioDuckingCallback callback) override {
    content::RenderFrameHost* guest_frame = page_handler_->GetGuestMainFrame();
    if (!guest_frame) {
      std::move(callback).Run(false);
      return;
    }
    AudioDucker* audio_ducker =
        AudioDucker::GetOrCreateForPage(guest_frame->GetPage());
    std::move(callback).Run(enabled ? audio_ducker->StartDuckingOtherAudio()
                                    : audio_ducker->StopDuckingOtherAudio());
  }

  void SetPanelDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas,
      SetPanelDraggableAreasCallback callback) override {
    if (!draggable_areas.empty()) {
      host().SetPanelDraggableAreas(page_handler_, draggable_areas);
    } else {
      // Default to the top bar area of the panel.
      // TODO(cuianthony): Define panel dimensions constants in shared location.
      host().SetPanelDraggableAreas(page_handler_, {{0, 0, 400, 80}});
    }
    std::move(callback).Run();
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
    if (!base::FeatureList::IsEnabled(features::kGlicClosedCaptioning)) {
      receiver_.ReportBadMessage(
          "Client should not be able to call SetClosedCaptioningSetting "
          "without the GlicClosedCaptioning feature enabled.");
      return;
    }
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
    pref_service_->SetBoolean(prefs::kGlicUserEnabledActuationOnWeb, enabled);
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
    std::move(callback).Run(
        pref_service_->GetBoolean(prefs::kGlicGeolocationEnabled) &&
        host().IsWidgetShowing(this));
  }

  void SetContextAccessIndicator(bool enabled) override {
    host().SetContextAccessIndicator(page_handler_, enabled);
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
    result->email = base::UTF16ToUTF8(entry->GetUserName());
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
    glic_service_->GetAuthController().ForceSyncCookies(std::move(callback));
  }

  void LogBeginAsyncEvent(uint64_t event_async_id,
                          int32_t task_id,
                          const std::string& event,
                          const std::string& details) override {
    journal_handler_.LogBeginAsyncEvent(event_async_id, task_id, event,
                                        details);
  }

  void LogEndAsyncEvent(uint64_t event_async_id,
                        const std::string& details) override {
    journal_handler_.LogEndAsyncEvent(glic_service_->metrics()->current_model(),
                                      event_async_id, details);
  }

  void LogInstantEvent(int32_t task_id,
                       const std::string& event,
                       const std::string& details) override {
    journal_handler_.LogInstantEvent(task_id, event, details);
  }

  void JournalClear() override { journal_handler_.Clear(); }

  void JournalSnapshot(bool clear_journal,
                       JournalSnapshotCallback callback) override {
    journal_handler_.Snapshot(clear_journal, std::move(callback));
  }

  void JournalStart(uint64_t max_bytes, bool capture_screenshots) override {
    journal_handler_.Start(max_bytes, capture_screenshots);
  }

  void JournalStop() override { journal_handler_.Stop(); }

  void JournalRecordFeedback(bool positive,
                             const std::string& reason) override {
    journal_handler_.RecordFeedback(positive, reason);
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnUserInputSubmitted(mojom::WebClientMode mode) override {
    glic_service_->OnUserInputSubmitted(mode);
    glic_service_->metrics()->OnUserInputSubmitted(mode);
    if (auto* instance_metrics = host().instance_metrics()) {
      instance_metrics->OnUserInputSubmitted(mode);
    }

    // TODO(crbug.com/462769104): move this to a non-metrics API.
    sharing_manager().OnConversationTurnSubmitted();
  }

  void OnContextUploadStarted() override {
    glic_service_->metrics()->OnContextUploadStarted();
  }

  void OnContextUploadCompleted() override {
    glic_service_->metrics()->OnContextUploadCompleted();
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnReaction(mojom::MetricUserInputReactionType reaction_type) override {
    glic_service_->metrics()->OnReaction(reaction_type);
    if (auto* instance_metrics = host().instance_metrics()) {
      instance_metrics->OnReaction(reaction_type);
    }
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnResponseStarted() override {
    glic_service_->metrics()->OnResponseStarted();
    if (auto* instance_metrics = host().instance_metrics()) {
      instance_metrics->OnResponseStarted();
      instance_metrics->RecordAttachedContextTabCount(
          sharing_manager().GetNumPinnedTabs());
    }
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnResponseStopped(mojom::OnResponseStoppedDetailsPtr details) override {
    mojom::ResponseStopCause cause = mojom::ResponseStopCause::kUnknown;
    if (details) {
      cause = details->cause;
    }
    glic_service_->metrics()->OnResponseStopped(cause);
    if (auto* instance_metrics = host().instance_metrics()) {
      instance_metrics->OnResponseStopped(cause);
    }
  }

  void OnSessionTerminated() override {
    glic_service_->metrics()->OnSessionTerminated();
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnTurnCompleted(glic::mojom::WebClientModel model,
                       base::TimeDelta duration) override {
    glic_service_->metrics()->OnTurnCompleted(model, duration);
    if (auto* instance_metrics = host().instance_metrics()) {
      instance_metrics->OnTurnCompleted(model, duration);
    }
  }

  void OnModelChanged(glic::mojom::WebClientModel model) override {
    glic_service_->metrics()->OnModelChanged(model);
  }

  void OnRecordUseCounter(uint16_t counter) override {
    glic_service_->metrics()->OnRecordUseCounter(counter);
  }

  void OnResponseRated(bool positive) override {
    glic_service_->metrics()->OnResponseRated(positive);
  }

  void OnClosedCaptionsShown() override {
    if (!base::FeatureList::IsEnabled(features::kGlicClosedCaptioning)) {
      receiver_.ReportBadMessage(
          "Client should not be able to call OnClosedCaptionsShown "
          "without the GlicClosedCaptioning feature enabled.");
      return;
    }

    glic_service_->metrics()->LogClosedCaptionsShown();
  }

  void ScrollTo(mojom::ScrollToParamsPtr params,
                ScrollToCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
      receiver_.ReportBadMessage(
          "Client should not be able to call ScrollTo without the GlicScrollTo "
          "feature enabled.");
      return;
    }
    annotation_manager_->ScrollTo(std::move(params), std::move(callback),
                                  &host());
  }

  void DropScrollToHighlight() override {
    if (!base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
      receiver_.ReportBadMessage(
          "Client should not be able to call DropScrollToHighlight without the "
          "GlicScrollTo feature enabled.");
      return;
    }
    annotation_manager_->RemoveAnnotation(
        mojom::ScrollToErrorReason::kDroppedByWebClient);
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
    sharing_manager().SubscribeToPinCandidates(std::move(options),
                                               std::move(observer));
  }

  void OnViewChanged(mojom::ViewChangedNotificationPtr notification) override {
    host().OnViewChanged(this, notification->current_view);
  }

  // GlicWindowController::StateObserver implementation.
  void PanelStateChanged(
      const glic::mojom::PanelState& panel_state,
      const GlicWindowController::PanelStateContext& context) override {
    web_client_->NotifyPanelStateChange(panel_state.Clone());
  }

  // GlicWebClientAccess implementation.

  void FloatingPanelCanAttachChanged(bool can_attach) override {
    floating_panel_can_attach_ = can_attach;
    NotifyCanAttachChanged();
  }

  void PanelWillOpen(glic::mojom::PanelOpeningDataPtr panel_opening_data,
                     PanelWillOpenCallback done) override {
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
  }

  void PanelWasClosed(base::OnceClosure done) override {
    web_client_->NotifyPanelWasClosed(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(done)));
  }

  void PanelStateChanged(const glic::mojom::PanelState& panel_state) override {
    web_client_->NotifyPanelStateChange(panel_state.Clone());
  }

  void ManualResizeChanged(bool resizing) override {
    web_client_->NotifyManualResizeChanged(resizing);
  }

  void RequestViewChange(mojom::ViewChangeRequestPtr request) override {
    web_client_->RequestViewChange(std::move(request));
  }

  void NotifyAdditionalContext(mojom::AdditionalContextPtr context) override {
    web_client_->NotifyAdditionalContext(std::move(context));
  }

  // BrowserAttachmentObserver implementation.
  void CanAttachToBrowserChanged(bool can_attach) override {
    NotifyCanAttachChanged();
  }
  // ActiveStateCalculator implementation.
  void ActiveStateChanged(bool is_active) override {
    if (web_client_) {
      web_client_->NotifyPanelActiveChange(is_active);
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
    if (!contextual_cueing::IsZeroStateSuggestionsEnabled()) {
      receiver_.ReportBadMessage(
          "Client should not call "
          "GetZeroStateSuggestionsForFocusedTab "
          "without the GlicZeroStateSuggestions feature enabled.");
      return;
    }

    // TODO(crbug.com/424472586): Pass supported tools to service from web
    // client.
    host().instance_delegate().FetchZeroStateSuggestions(
        is_fre.value_or(false),
        /*supported_tools=*/{},
        base::BindOnce(
            [](GetZeroStateSuggestionsForFocusedTabCallback callback,
               base::TimeTicks start,
               glic::mojom::ZeroStateSuggestionsPtr suggestions) {
              base::UmaHistogramTimes(
                  "Glic.Api.FetchZeroStateSuggestionsLatency",
                  base::TimeTicks::Now() - start);
              std::move(callback).Run(std::move(suggestions));
            },
            std::move(callback), base::TimeTicks::Now()));
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

  void OnPinningChanged(
      const std::vector<content::WebContents*>& pinned_contents) {
    std::vector<glic::mojom::TabDataPtr> tab_data;
    for (content::WebContents* web_contents : pinned_contents) {
      tab_data.push_back(CreateTabData(web_contents));
    }
    web_client_->NotifyPinnedTabsChanged(std::move(tab_data));
  }

  void SubscribeToPageMetadata(
      int32_t tab_id,
      const std::vector<std::string>& names,
      SubscribeToPageMetadataCallback callback) override {
    page_metadata_manager_->SubscribeToPageMetadata(tab_id, names,
                                                    std::move(callback));
  }

  void OnPinnedTabDataChanged(const TabDataChange& change) {
    if (!change.tab_data) {
      return;
    }
    web_client_->NotifyPinnedTabDataChanged(change.tab_data->Clone());
  }

  void OnTabDataChanged(const TabDataChange& change) {
    if (!change.tab_data) {
      return;
    }
    web_client_->NotifyTabDataChanged(change.tab_data->Clone());
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

 private:
  bool ComputeCanAttach() const {
    if (GlicEnabling::IsMultiInstanceEnabled()) {
      return floating_panel_can_attach_;
    }
    return floating_panel_can_attach_ ||
           (browser_attach_observation_ &&
            browser_attach_observation_->CanAttachToBrowser());
  }

  void NotifyCanAttachChanged() {
    if (!web_client_) {
      return;
    }
    web_client_->NotifyPanelCanAttachChange(ComputeCanAttach());
  }

  void Uninstall() {
    page_metadata_manager_.reset();
    SetAudioDucking(false, base::DoNothing());
    host().UnsetWebClient(this);
    pref_change_registrar_.Reset();
    local_state_pref_change_registrar_.Reset();
    host().RemovePanelStateObserver(this);
    focus_changed_subscription_ = {};
    pinned_tabs_changed_subscription_ = {};
    pinned_tab_data_changed_subscription_ = {};
    browser_attach_observation_.reset();
    if (glic_service_->zero_state_suggestions_manager()) {
      glic_service_->zero_state_suggestions_manager()->Reset();
    }
  }

  void WebClientDisconnected() { Uninstall(); }

  void OnPrefChanged(const std::string& pref_name) {
    bool is_enabled = pref_service_->GetBoolean(pref_name);
    if (pref_name == prefs::kGlicMicrophoneEnabled) {
      web_client_->NotifyMicrophonePermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicGeolocationEnabled) {
      web_client_->NotifyLocationPermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicTabContextEnabled) {
      web_client_->NotifyTabContextPermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicClosedCaptioningEnabled) {
      web_client_->NotifyClosedCaptioningSettingChanged(is_enabled);
    } else if (pref_name == prefs::kGlicDefaultTabContextEnabled) {
      web_client_->NotifyDefaultTabContextPermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicUserEnabledActuationOnWeb) {
      web_client_->NotifyActuationOnWebSettingChanged(is_enabled);
    } else {
      DCHECK(false) << "Unknown Glic permission pref changed: " << pref_name;
    }
  }

  void OnLocalStatePrefChanged(const std::string& pref_name) {
    if (pref_name == prefs::kGlicLauncherHotkey) {
      web_client_->NotifyOsHotkeyStateChanged(GetHotkeyString());
    } else {
      CHECK(false) << "Unknown local state pref changed: " << pref_name;
    }
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

  bool ShouldDoApiActivationGating() const {
    return base::FeatureList::IsEnabled(features::kGlicApiActivationGating) &&
           !active_state_calculator_.IsActive();
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

  void NotifyActorTaskStateChanged(actor::TaskId task_id,
                                   actor::ActorTask::State task_state) {
    const mojom::ActorTaskState state = [&]() {
      switch (task_state) {
        case actor::ActorTask::State::kCreated:
        case actor::ActorTask::State::kReflecting:
        case actor::ActorTask::State::kWaitingOnUser:
          return mojom::ActorTaskState::kIdle;
        case actor::ActorTask::State::kActing:
          return mojom::ActorTaskState::kActing;
        case actor::ActorTask::State::kPausedByActor:
        case actor::ActorTask::State::kPausedByUser:
          return mojom::ActorTaskState::kPaused;
        case actor::ActorTask::State::kCancelled:
        case actor::ActorTask::State::kFinished:
        case actor::ActorTask::State::kFailed:
          return mojom::ActorTaskState::kStopped;
      }
    }();
    web_client_->NotifyActorTaskStateChanged(task_id.value(), state);
  }

  void RequestToShowCredentialSelectionDialog(
      actor::TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials,
      actor::ActorTaskDelegate::CredentialSelectedCallback callback) override {
    // Note: mojom::<Type>Ptr is not copyable, meaning it can't be passed to the
    // argument of base::RepeatingCallbackList::Notify (who makes a copy of the
    // argument). All of the mojom::<Type>Ptr will be constructed locally before
    // being passed into the mojom interface.
    std::vector<actor::webui::mojom::CredentialPtr> mojo_credentials;
    for (const auto& credential : credentials) {
      mojo_credentials.push_back(actor::webui::mojom::Credential::New(
          credential.id.value(), base::UTF16ToUTF8(credential.username),
          base::UTF16ToUTF8(credential.source_site_or_app),
          credential.request_origin));
    }
    base::flat_map<std::string, SkBitmap> mojo_icons;
    for (const auto& [site_or_app, image] : icons) {
      CHECK(!image.IsEmpty());
      mojo_icons.insert({site_or_app, image.AsBitmap()});
    }
    auto dialog_request =
        actor::webui::mojom::SelectCredentialDialogRequest::New(
            task_id.value(),
            // TODO(crbug.com/440147814): `show_dialog` should be based on the
            // user granted permission duration.
            /*show_dialog=*/true, std::move(mojo_credentials),
            std::move(mojo_icons));

    web_client_->RequestToShowCredentialSelectionDialog(
        std::move(dialog_request), std::move(callback));
  }

  void RequestToShowUserConfirmationDialog(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      bool for_sensitive_origin,
      actor::ActorTaskDelegate::UserConfirmationDialogCallback callback)
      override {
    actor::webui::mojom::UserConfirmationDialogPayloadPtr payload = nullptr;
    payload = actor::webui::mojom::UserConfirmationDialogPayload::New(
        navigation_origin, for_sensitive_origin);
    web_client_->RequestToShowUserConfirmationDialog(
        actor::webui::mojom::UserConfirmationDialogRequest::New(
            std::move(payload)),
        std::move(callback));
  }

  void RequestToConfirmNavigation(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      actor::ActorTaskDelegate::NavigationConfirmationCallback callback)
      override {
    web_client_->RequestToConfirmNavigation(
        actor::webui::mojom::NavigationConfirmationRequest::New(
            task_id.value(), navigation_origin),
        std::move(callback));
  }

  void RequestToShowAutofillSuggestionsDialog(
      actor::TaskId task_id,
      std::vector<autofill::ActorFormFillingRequest> requests,
      actor::ActorTaskDelegate::AutofillSuggestionSelectedCallback
          on_autofill_suggestions_selected) override {
    std::vector<actor::webui::mojom::FormFillingRequestPtr> mojo_requests;
    for (const auto& request : requests) {
      auto mojo_request = actor::webui::mojom::FormFillingRequest::New();
      mojo_request->requested_data =
          static_cast<int64_t>(request.requested_data);
      for (const auto& suggestion : request.suggestions) {
        auto mojo_suggestion = actor::webui::mojom::AutofillSuggestion::New();
        mojo_suggestion->id = base::NumberToString(suggestion.id.value());
        mojo_suggestion->title = suggestion.title;
        mojo_suggestion->details = suggestion.details;
        if (suggestion.icon) {
          mojo_suggestion->icon = suggestion.icon->AsBitmap();
        }
        mojo_request->suggestions.push_back(std::move(mojo_suggestion));
      }
      mojo_requests.push_back(std::move(mojo_request));
    }

    auto dialog_request =
        actor::webui::mojom::SelectAutofillSuggestionsDialogRequest::New(
            task_id.value(), std::move(mojo_requests));

    web_client_->RequestToShowAutofillSuggestionsDialog(
        std::move(dialog_request), std::move(on_autofill_suggestions_selected));
  }

  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  raw_ptr<Profile> profile_;
  raw_ptr<GlicPageHandler> page_handler_;
  raw_ptr<GlicKeyedService> glic_service_;
  raw_ptr<GlicWindowController> window_controller_;
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
  mojo::Receiver<glic::mojom::WebClientHandler> receiver_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
  std::unique_ptr<BrowserAttachObservation> browser_attach_observation_;
  const std::unique_ptr<GlicAnnotationManager> annotation_manager_;
  std::unique_ptr<system_permission_settings::ScopedObservation>
      system_permission_settings_observation_;
  JournalHandler journal_handler_;
  std::unique_ptr<DebouncerDeduper> debouncer_deduper_;
  std::unique_ptr<PageMetadataManager> page_metadata_manager_;
  bool floating_panel_can_attach_ = false;
};

GlicPageHandler::GlicPageHandler(
    content::WebContents* webui_contents,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page)
    : webui_contents_(webui_contents),
      browser_context_(webui_contents->GetBrowserContext()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  host_ = GetGlicService()->host_manager().WebUIPageHandlerAdded(this);
  host_->AddPanelStateObserver(this);
  UpdatePageState(host().GetPanelState(web_client_handler_.get()).kind);
  subscriptions_.push_back(
      GetGlicService()->enabling().RegisterProfileReadyStateChanged(
          base::BindRepeating(&GlicPageHandler::UpdateProfileReadyState,
                              base::Unretained(this))));
  UpdateProfileReadyState();
}

GlicPageHandler::~GlicPageHandler() {
  host_->RemovePanelStateObserver(this);
  WebUiStateChanged(glic::mojom::WebUiState::kUninitialized);
  // `GlicWebClientHandler` holds a pointer back to us, so delete it first.
  web_client_handler_.reset();
  host_ = nullptr;
  GetGlicService()->host_manager().WebUIPageHandlerRemoved(this);
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
  GetGlicService()->GetAuthController().CheckAuthBeforeLoad(
      std::move(callback));
}

void GlicPageHandler::WebviewCommitted(const GURL& url) {
  // TODO(crbug.com/388328847): Remove this code once launch issues are ironed
  // out.
  if (url.DomainIs("login.corp.google.com") ||
      url.DomainIs("accounts.google.com")) {
    host().LoginPageCommitted(this);
  }
}

void GlicPageHandler::NotifyWindowIntentToShow() {
  page_->IntentToShow();
}

content::RenderFrameHost* GlicPageHandler::GetGuestMainFrame() {
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
  NavigateParams params(Profile::FromBrowserContext(browser_context_),
                        disabled_by_admin_link_url,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  host().ClosePanel(this);
  base::RecordAction(
      base::UserMetricsAction("Glic.DisabledByAdminPanelLinkClicked"));
}

void GlicPageHandler::SignInAndClosePanel() {
  GetGlicService()->GetAuthController().ShowReauthForAccount(base::BindOnce(
      &GlicWindowController::ShowAfterSignIn,
      // Unretained is safe because the keyed service owns the
      // auth controller and the window controller.
      base::Unretained(&GetGlicService()->window_controller()), nullptr));
  host().ClosePanel(this);
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
    const glic::mojom::PanelState& panel_state,
    const PanelStateContext& context) {
  UpdatePageState(panel_state.kind);
}

void GlicPageHandler::UpdateProfileReadyState() {
  page_->SetProfileReadyState(GlicEnabling::GetProfileReadyState(
      Profile::FromBrowserContext(browser_context_)));
}

void GlicPageHandler::UpdatePageState(mojom::PanelStateKind panelStateKind) {
  page_->UpdatePageState(panelStateKind);
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
