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
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/aggregated_journal_file_serializer.h"
#include "chrome/browser/actor/aggregated_journal_in_memory_serializer.h"
#include "chrome/browser/actor/autofill_selection_dialog_event_handler.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/context/glic_tab_data_observer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_annotation_manager.h"
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
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
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
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/public/skills_metrics.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "pdf/buildflags.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "ui/base/base_window.h"
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

mojom::FormFactor GetGlicFormFactor(ui::DeviceFormFactor form_factor) {
  switch (form_factor) {
    case ui::DEVICE_FORM_FACTOR_DESKTOP:
      return mojom::FormFactor::kDesktop;
    case ui::DEVICE_FORM_FACTOR_PHONE:
      return mojom::FormFactor::kPhone;
    case ui::DEVICE_FORM_FACTOR_TABLET:
      return mojom::FormFactor::kTablet;
    case ui::DEVICE_FORM_FACTOR_TV:
    case ui::DEVICE_FORM_FACTOR_AUTOMOTIVE:
    case ui::DEVICE_FORM_FACTOR_FOLDABLE:
    case ui::DEVICE_FORM_FACTOR_XR:
      return mojom::FormFactor::kUnknown;
  }
}

#if BUILDFLAG(IS_MAC)
constexpr mojom::Platform kPlatform = mojom::Platform::kMacOS;
#elif BUILDFLAG(IS_WIN)
constexpr mojom::Platform kPlatform = mojom::Platform::kWindows;
#elif BUILDFLAG(IS_LINUX)
constexpr mojom::Platform kPlatform = mojom::Platform::kLinux;
#elif BUILDFLAG(IS_CHROMEOS)
constexpr mojom::Platform kPlatform = mojom::Platform::kChromeOS;
#elif BUILDFLAG(IS_ANDROID)
constexpr mojom::Platform kPlatform = mojom::Platform::kAndroid;
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

// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
mojom::SkillSource ToMojomSkillSource(sync_pb::SkillSource source) {
  switch (source) {
    case sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN:
      return mojom::SkillSource::kUnknown;
    case sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY:
      return mojom::SkillSource::kFirstParty;
    case sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED:
      return mojom::SkillSource::kUserCreated;
    case sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY:
      return mojom::SkillSource::kDerivedFromFirstParty;
  }
}

mojom::SkillPreviewPtr ToMojomSkillPreview(const skills::Skill* skill) {
  if (!skill) {
    return nullptr;
  }
  return mojom::SkillPreview::New(skill->id, skill->name, skill->icon,
                                  ToMojomSkillSource(skill->source),
                                  skill->description);
}

sync_pb::SkillSource FromMojomSkillSource(mojom::SkillSource source) {
  switch (source) {
    case mojom::SkillSource::kUnknown:
      return sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN;
    case mojom::SkillSource::kFirstParty:
      return sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
    case mojom::SkillSource::kUserCreated:
      return sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;
    case mojom::SkillSource::kDerivedFromFirstParty:
      return sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

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

    // attached_browser is always null in Multi-instance, and ANDROID implies
    // Multi-instance.
#if !BUILDFLAG(IS_ANDROID)
    if (attached_browser_ && !IsDeleteScheduled(attached_browser_)) {
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
#endif
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
    if (IsDeleteScheduled(attached_browser_)) {
      return false;
    }

    return glic::IsActive(attached_browser_);
  }

  base::OneShotTimer calc_timer_;
  std::vector<base::CallbackListSubscription> attached_browser_subscriptions_;

  raw_ptr<Host> host_;
  base::ObserverList<Observer> observers_;
  glic::mojom::PanelStateKind panel_state_kind_;
  bool is_active_ = false;
  raw_ptr<BrowserWindowInterface> attached_browser_ = nullptr;
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
    // If there is a matching ID make sure it terminates before the new event
    // is created.
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

  void LogEndAsyncEvent(uint64_t event_async_id, const std::string& details) {
    auto it = active_journal_events_.find(event_async_id);
    if (it != active_journal_events_.end()) {
      it->second->EndEntry(
          actor::JournalDetailsBuilder().Add("end_details", details).Build());

      if (!it->second->GetTaskId().is_null()) {
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
// NEEDS_ANDROID_IMPL: FeedbackUploaderFactoryChrome
#if !BUILDFLAG(IS_ANDROID)
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
#endif
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

mojom::ProfileEnablementPtr BuildProfileEnablement(
    content::BrowserContext* browser_context,
    const GlicActorPolicyChecker& actor_policy_checker) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile);

  auto result = mojom::ProfileEnablement::New();
  result->feature_disabled = enablement.feature_disabled;
  result->not_regular_profile = enablement.not_regular_profile;
  result->not_rolled_out = enablement.not_rolled_out;
  result->primary_account_not_capable = enablement.primary_account_not_capable;
  result->primary_account_not_fully_signed_in =
      enablement.primary_account_not_fully_signed_in;
  result->disallowed_by_chrome_policy = enablement.disallowed_by_chrome_policy;
  result->disallowed_by_remote_admin = enablement.disallowed_by_remote_admin;
  result->disallowed_by_remote_other = enablement.disallowed_by_remote_other;
  result->not_consented = enablement.not_consented;
  result->live_disallowed = enablement.live_disallowed;
  result->share_image_disallowed = enablement.share_image_disallowed;
  result->actuation_not_consented =
      profile->GetPrefs()->GetBoolean(prefs::kGlicUserEnabledActuationOnWeb) ==
      false;

  using CannotActReason = GlicActorPolicyChecker::CannotActReason;
  if (actor_policy_checker.CanActOnWeb()) {
    result->actuation_eligibility = mojom::ActuationEligibility::kEligible;
  } else {
    switch (actor_policy_checker.CannotActOnWebReason()) {
      case CannotActReason::kAccountCapabilityIneligible:
        result->actuation_eligibility =
            mojom::ActuationEligibility::kMissingAccountCapability;
        break;
      case CannotActReason::kAccountMissingChromeBenefits:
        result->actuation_eligibility =
            mojom::ActuationEligibility::kMissingChromeBenefits;
        break;
      case CannotActReason::kDisabledByPolicy:
        result->actuation_eligibility =
            mojom::ActuationEligibility::kDisabledByPolicy;
        break;
      case CannotActReason::kEnterpriseWithoutManagement:
        result->actuation_eligibility =
            mojom::ActuationEligibility::kEnterpriseWithoutManagement;
        break;
      case CannotActReason::kNone:
        NOTREACHED();
    }
  }

  return result;
}

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
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
                             public skills::SkillsService::Observer,
#endif  //  !BUILDFLAG(IS_ANDROID)
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
            std::make_unique<GlicAnnotationManager>(glic_service_)) {
    if (base::FeatureList::IsEnabled(features::kGlicActor)) {
      journal_handler_ = std::make_unique<JournalHandler>(profile_);
    }
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

    NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.opened_by_another_window = true;
    params.window_features.bounds = gfx::Rect(x, y, popup_width, popup_height);
    DoNavigate(&params);
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
    pref_change_registrar_.Add(
        prefs::kGlicCompletedFre,
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

#if !BUILDFLAG(IS_ANDROID)  // single instance not implemented on android
    if (!GlicEnabling::IsMultiInstanceEnabled()) {
      browser_attach_observation_ = ObserveBrowserForAttachment(profile_, this);
    }
#endif

    system_permission_settings_observation_ =
        system_permission_settings::Observe(base::BindRepeating(
            &GlicWebClientHandler::OnOsPermissionSettingChanged,
            base::Unretained(this)));

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
    if (base::FeatureList::IsEnabled(features::kGlicActor)) {
      if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
        actor_task_state_changed_subscription_ =
            actor_service->AddTaskStateChangedCallback(base::BindRepeating(
                &GlicWebClientHandler::NotifyActorTaskStateChanged,
                base::Unretained(this)));
      }

      // CallbackListSubscription prevents these callbacks from being invoked
      // when this object is destructed.
      act_on_web_capability_changed_subscription_ =
          glic_service_->AddActOnWebCapabilityChangedCallback(
              base::BindRepeating(
                  &GlicWebClientHandler::NotifyActOnWebCapabilityChanged,
                  base::Unretained(this)));
    }

    // NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only
    // restrictions from Skills backend.
    if (base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
      skills_service_ = skills::SkillsServiceFactory::GetForProfile(profile_);
      if (skills_service_) {
        skills_service_->AddObserver(this);
      }
    }
#endif

    auto state = glic::mojom::WebClientInitialState::New();
    state->chrome_version = version_info::GetVersion();
    state->platform = kPlatform;
    state->form_factor = GetGlicFormFactor(ui::GetDeviceFormFactor());
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
    state->enable_cached_get_user_profile_info = base::FeatureList::IsEnabled(
        features::kGlicEnableCachedGetUserProfileInfo);

    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
    local_state_pref_change_registrar_.Add(
        prefs::kGlicLauncherHotkey,
        base::BindRepeating(&GlicWebClientHandler::OnLocalStatePrefChanged,
                            base::Unretained(this)));
    state->hotkey = GetHotkeyString();
#endif
    state->enable_default_tab_context_setting_feature =
        base::FeatureList::IsEnabled(features::kGlicDefaultTabContextSetting);
    state->default_tab_context_setting_enabled =
        pref_service_->GetBoolean(prefs::kGlicDefaultTabContextEnabled);
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

    if (base::FeatureList::IsEnabled(features::kAutoOpenGlicForPdf)) {
      state->host_capabilities.push_back(mojom::HostCapability::kPdfZeroState);
    }

    const mojom::InvocationSource invocation_source =
        host().invocation_source().value_or(
            mojom::InvocationSource::kUnsupported);

    const bool should_bypass_fre_ui =
        GlicEnabling::ShouldBypassFreUi(profile_, invocation_source);

    if (!should_bypass_fre_ui &&
        GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(profile_)) {
      int arm = features::kGlicTrustFirstOnboardingArmParam.Get();
      if (arm == 1) {
        state->host_capabilities.push_back(
            mojom::HostCapability::kTrustFirstOnboardingArm1);
      } else if (arm == 2) {
        state->host_capabilities.push_back(
            mojom::HostCapability::kTrustFirstOnboardingArm2);
      }
    }
    if (GlicEnabling::IsShareImageEnabledForProfile(profile_)) {
      // TODO(b:468877076): Ideally this would be a dynamic capability.
      state->host_capabilities.push_back(
          mojom::HostCapability::kShareAdditionalImageContext);
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
      state->can_act_on_web =
          glic_service_->actor_policy_checker().CanActOnWeb();
    }
    state->enable_activate_tab = base::FeatureList::IsEnabled(
        glic::mojom::features::kGlicActivateTabApi);
    state->enable_get_tab_by_id =
        base::FeatureList::IsEnabled(features::kGlicGetTabByIdApi);
    state->enable_open_password_manager_settings_page =
        base::FeatureList::IsEnabled(
            features::kGlicOpenPasswordManagerSettingsPageApi);
    state->enable_trust_first_onboarding =
        !should_bypass_fre_ui &&
        GlicEnabling::IsTrustFirstOnboardingEnabledForProfile(profile_);
    state->onboarding_completed =
        GlicEnabling::HasConsentedForProfile(profile_);
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
    state->enable_skills =
        base::FeatureList::IsEnabled(features::kSkillsEnabled);
#endif

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
    if (tab) {
      host()
          .instance_metrics_backwards_compatibility()
          .DidRequestContextFromTab(*tab);
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
    std::move(callback).Run(sharing_manager().PinTabs(tab_handles, trigger));
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
    std::move(callback).Run(sharing_manager().UnpinTabs(tab_handles, trigger));
  }

  void UnpinAllTabs(mojom::UnpinTabsOptionsPtr options) override {
    GlicUnpinTrigger trigger = GlicUnpinTrigger::kWebClientUnknown;
    if (options) {
      trigger = FromMojomUnpinTrigger(options->unpin_trigger);
    }
    sharing_manager().UnpinAllTabs(trigger);
  }

  void CreateTask(actor::webui::mojom::TaskOptionsPtr options,
                  CreateTaskCallback callback) override {
    host().instance_delegate().CreateTask(nullptr, std::move(options),
                                          std::move(callback));
  }

  void PerformActions(const std::vector<uint8_t>& actions_proto,
                      PerformActionsCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "PerformActions cannot be called without GlicActor enabled.");
      return;
    }
    host().instance_delegate().PerformActions(actions_proto,
                                              std::move(callback));
  }

  void CancelActions(int32_t task_id, CancelActionsCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicActor)) {
      receiver_.ReportBadMessage(
          "CancelActions cannot be called without GlicActor enabled.");
      return;
    }
    host().instance_delegate().CancelActions(actor::TaskId(task_id),
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

  void CreateSkill(mojom::CreateSkillRequestPtr request,
                   CreateSkillCallback callback) override {
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
    auto scoped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);

    if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
      receiver_.ReportBadMessage(
          "CreateSkill cannot be called without Skills enabled.");
      return;
    }
    // There are three scenarios:
    // 1. Users click the + button in the / menu: no field is set.
    // 2. Users click the save as a skill button: only prompt is set.
    // 3. Users edit a 1P skill: all fields are set.
    // TODO(https://crbug.com/479950619): consider using mojom source enum
    // directly in skills::Skill..
    skills::Skill skill(request->id, request->name, request->icon,
                        request->prompt, request->description,
                        FromMojomSkillSource(request->source));
    host().skills_manager().LaunchSkillsDialog(profile_, std::move(skill),
                                               std::move(scoped_callback));
#else
    receiver_.ReportBadMessage("CreateSkill isn't supported on Android.");
#endif  //  !BUILDFLAG(IS_ANDROID)
  }

  void UpdateSkill(mojom::UpdateSkillRequestPtr request,
                   UpdateSkillCallback callback) override {
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
    auto scoped_callback =
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);

    if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
      receiver_.ReportBadMessage(
          "UpdateSkill cannot be called without Skills enabled.");
      return;
    }
    // Get skill by ID from the SkillsService.
    skills::SkillsService* skills_service =
        skills::SkillsServiceFactory::GetForProfile(profile_);
    if (const skills::Skill* skill =
            skills_service->GetSkillById(request->id)) {
      host().skills_manager().LaunchSkillsDialog(profile_, *skill,
                                                 std::move(scoped_callback));
    }
#else
    receiver_.ReportBadMessage("UpdateSkill isn't supported on Android.");
#endif  //  !BUILDFLAG(IS_ANDROID)
  }

  void ShowManageSkillsUi() override {
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
    if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
      receiver_.ReportBadMessage(
          "ShowManageSkillsUi cannot be called without Skills enabled.");
      return;
    }

    host().skills_manager().ShowManageSkillsUi();
#else
    receiver_.ReportBadMessage(
        "ShowManageSkillsUi isn't supported on Android.");
#endif  //  !BUILDFLAG(IS_ANDROID)
  }

  void GetSkill(const std::string& id, GetSkillCallback callback) override {
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
    if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
      receiver_.ReportBadMessage(
          "GetSkill cannot be called without Skills enabled.");
      return;
    }
    mojom::SkillPtr skill = GetSkillById(id);
    std::move(callback).Run(std::move(skill));
#else
    receiver_.ReportBadMessage("GetSkill isn't supported on Android.");
#endif  //  !BUILDFLAG(IS_ANDROID)
  }

  void RecordSkillsWebClientEvent(
      glic::mojom::SkillsWebClientEvent action) override {
    if (auto* instance_metrics = host().instance_metrics()) {
      instance_metrics->RecordSkillsWebClientEvent(action);
    }
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
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL: CaptureRegion
    const FocusedTabData& focus = sharing_manager().GetFocusedTabData();
    // Prioritize the focused tab, but fall back to the unfocused tab if one is
    // available. This is useful in cases where the active tab is not
    // "focusable" by Glic (e.g. chrome:// pages).
    tabs::TabInterface* active_tab =
        focus.is_focus() ? focus.focus() : focus.unfocused_tab();
    glic_service_->CaptureRegion(active_tab, std::move(observer));
#else
    NOTIMPLEMENTED();
#endif
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
    glic_service_->GetAuthController().ForceSyncCookies(std::move(callback));
  }

  void LogBeginAsyncEvent(uint64_t event_async_id,
                          int32_t task_id,
                          const std::string& event,
                          const std::string& details) override {
    if (journal_handler_) {
      journal_handler_->LogBeginAsyncEvent(event_async_id, task_id, event,
                                           details);
    }
  }

  void LogEndAsyncEvent(uint64_t event_async_id,
                        const std::string& details) override {
    if (journal_handler_) {
      journal_handler_->LogEndAsyncEvent(event_async_id, details);
    }
  }

  void LogInstantEvent(int32_t task_id,
                       const std::string& event,
                       const std::string& details) override {
    if (journal_handler_) {
      journal_handler_->LogInstantEvent(task_id, event, details);
    }
  }

  void JournalClear() override {
    if (journal_handler_) {
      journal_handler_->Clear();
    }
  }

  void JournalSnapshot(bool clear_journal,
                       JournalSnapshotCallback callback) override {
    if (journal_handler_) {
      journal_handler_->Snapshot(clear_journal, std::move(callback));
    }
  }

  void JournalStart(uint64_t max_bytes, bool capture_screenshots) override {
    if (journal_handler_) {
      journal_handler_->Start(max_bytes, capture_screenshots);
    }
  }

  void JournalStop() override {
    if (journal_handler_) {
      journal_handler_->Stop();
    }
  }

  void JournalRecordFeedback(bool positive,
                             const std::string& reason) override {
    if (journal_handler_) {
      journal_handler_->RecordFeedback(positive, reason);
    }
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnUserInputSubmitted(mojom::WebClientMode mode) override {
    glic_service_->OnUserInputSubmitted(mode);
    host().instance_metrics_backwards_compatibility().OnUserInputSubmitted(
        mode);

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
    host().instance_metrics_backwards_compatibility().OnReaction(reaction_type);
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnResponseStarted() override {
    host().instance_metrics_backwards_compatibility().OnResponseStarted();
    if (auto* instance_metrics = host().instance_metrics()) {
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
    host().instance_metrics_backwards_compatibility().OnResponseStopped(cause);
  }

  void OnSessionTerminated() override {
    glic_service_->metrics()->OnSessionTerminated();
  }

  // TODO(crbug.com/450026474): Remove call to GlicMetrics once
  // non-profile-scoped metrics are logged entirely from GlicInstanceMetrics.
  void OnTurnCompleted(glic::mojom::WebClientModel model,
                       base::TimeDelta duration) override {
    host().instance_metrics_backwards_compatibility().OnTurnCompleted(model,
                                                                      duration);
  }

  void OnRecordUseCounter(uint16_t counter) override {
    glic_service_->metrics()->OnRecordUseCounter(counter);
  }

  void OnResponseRated(bool positive) override {
    glic_service_->metrics()->OnResponseRated(positive);
  }

  void OnClosedCaptionsShown() override {
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
                                  &host(), this);
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

  void SetOnboardingCompleted() override {
    glic_service_->metrics()->OnTrustFirstOnboardingAccept();
    pref_service_->SetInteger(prefs::kGlicCompletedFre,
                              static_cast<int>(prefs::FreStatus::kCompleted));

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
    GlicLauncherConfiguration::CheckDefaultBrowserToEnableLauncher();

    Browser* browser = chrome::FindTabbedBrowser(profile_, false);
    if (auto* interface = BrowserUserEducationInterface::From(browser)) {
      interface->NotifyAdditionalConditionEvent(
          feature_engagement::events::kGlicOnboardingCompleted);
    }
#endif  // !BUILDFLAG(IS_ANDROID)
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
  }

  void PanelWasClosed(base::OnceClosure done) override {
    host().SetInvocationSource(mojom::InvocationSource::kUnsupported);
    web_client_->NotifyPanelWasClosed(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(done)));
  }

  void StopMicrophone(base::OnceClosure done) override {
    web_client_->StopMicrophone(std::move(done));
  }

  void PanelStateChanged(const glic::mojom::PanelState& panel_state) override {
    web_client_->NotifyPanelStateChange(panel_state.Clone());
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

  void NotifySkillToInvokeChanged(mojom::SkillPtr skill) override {
    web_client_->NotifySkillToInvokeChanged(std::move(skill));
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
      tab_data.push_back(
          CreateTabData(tabs::TabInterface::GetFromContents(web_contents)));
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

  void NotifyContextualSkillPreviewsChanged(
      std::vector<mojom::SkillPreviewPtr> contextual_skill_previews) override {
    web_client_->NotifyContextualSkillPreviewsChanged(
        std::move(contextual_skill_previews));
  }

  void Invoke(mojom::InvokeOptionsPtr options,
              base::OnceClosure callback) override {
    web_client_->Invoke(std::move(options), std::move(callback));
  }

// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
  // SkillsService::Observer implementation.
  void OnSkillUpdated(std::string_view skill_id,
                      skills::SkillsService::UpdateSource update_source,
                      bool is_position_changed) override {
    if (!web_client_) {
      return;
    }

    if (is_position_changed) {
      // Update all the skill previews for simplicity as updating the position
      // is not frequent.
      host().skills_manager().UpdateSkillPreviews(std::nullopt);
      web_client_->NotifySkillPreviewsChanged(GetSkillPreviewsList());
      return;
    }

    mojom::SkillPtr skill = GetSkillById(skill_id);
    if (!skill) {
      web_client_->NotifySkillDeleted(skill_id.data());
    } else {
      web_client_->NotifySkillPreviewChanged(std::move(skill->preview));
    }
  }

  void OnStatusChanged() override {
    if (!web_client_) {
      return;
    }
    host().skills_manager().UpdateSkillPreviews(std::nullopt);
    web_client_->NotifySkillPreviewsChanged(GetSkillPreviewsList());
  }
#endif  // !BUILDFLAG(IS_ANDROID)

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
    browser_attach_observation_.reset();
    if (glic_service_->zero_state_suggestions_manager()) {
      glic_service_->zero_state_suggestions_manager()->Reset();
    }
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
    if (skills_service_) {
      skills_service_->RemoveObserver(this);
    }
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void WebClientDisconnected() { Uninstall(); }

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
    } else if (pref_name == prefs::kGlicUserEnabledActuationOnWeb) {
      web_client_->NotifyActuationOnWebSettingChanged(
          pref_service_->GetBoolean(pref_name));
    } else if (pref_name == prefs::kGlicCompletedFre) {
      web_client_->NotifyOnboardingCompletedChanged(
          pref_service_->GetInteger(prefs::kGlicCompletedFre) ==
          static_cast<int>(prefs::FreStatus::kCompleted));
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
    auto cred_type_to_mojo = [](actor_login::CredentialType type) {
      switch (type) {
        case actor_login::CredentialType::kPassword:
          return actor::webui::mojom::CredentialType::kPassword;
        case actor_login::CredentialType::kFederated:
          return actor::webui::mojom::CredentialType::kFederated;
      }
    };

    // Note: mojom::<Type>Ptr is not copyable, meaning it can't be passed to the
    // argument of base::RepeatingCallbackList::Notify (who makes a copy of the
    // argument). All of the mojom::<Type>Ptr will be constructed locally before
    // being passed into the mojom interface.
    std::vector<actor::webui::mojom::CredentialPtr> mojo_credentials;
    for (const auto& credential : credentials) {
      mojo_credentials.push_back(actor::webui::mojom::Credential::New(
          credential.id.value(), base::UTF16ToUTF8(credential.username),
          base::UTF16ToUTF8(credential.source_site_or_app),
          credential.request_origin,
          base::UTF16ToUTF8(credential.display_origin),
          cred_type_to_mojo(credential.type)));
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
      base::WeakPtr<actor::AutofillSelectionDialogEventHandler> event_handler,
      actor::ActorTaskDelegate::AutofillSuggestionSelectedCallback
          on_autofill_suggestions_selected) override {
    autofill_selection_event_handler_ = std::move(event_handler);

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

  void AutofillSuggestionDialogOnFormPresented(
      int32_t task_id,
      actor::webui::mojom::AutofillSuggestionDialogOnFormPresentedParamsPtr
          params) override {
    if (autofill_selection_event_handler_) {
      autofill_selection_event_handler_->OnFormPresented(std::move(params));
    }
  }

  void AutofillSuggestionDialogOnFormPreviewChanged(
      int32_t task_id,
      actor::webui::mojom::AutofillSuggestionDialogOnFormPreviewChangedParamsPtr
          params) override {
    if (autofill_selection_event_handler_) {
      autofill_selection_event_handler_->OnFormPreviewChanged(
          std::move(params));
    }
  }

  void AutofillSuggestionDialogOnFormConfirmed(
      int32_t task_id,
      actor::webui::mojom::AutofillSuggestionDialogOnFormConfirmedParamsPtr
          params) override {
    if (autofill_selection_event_handler_) {
      autofill_selection_event_handler_->OnFormConfirmed(std::move(params));
    }
  }

  mojom::SkillPtr GetSkillById(std::string_view skill_id) {
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
    glic::mojom::SkillPtr contextual_skill =
        host().skills_manager().GetContextualSkill(skill_id);
    if (contextual_skill) {
      return contextual_skill;
    }
    if (!skills_service_) {
      return nullptr;
    }
    const skills::Skill* skill = skills_service_->GetSkillById(skill_id);
    if (!skill) {
      return nullptr;
    }
    // We should only set the source_skill_id if the skill was derived from
    // another skill.
    return mojom::Skill::New(
        ToMojomSkillPreview(skill), skill->prompt,
        skill->source ==
                sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY
            ? skill->source_skill_id
            : std::string());
#else
    return nullptr;
#endif  //  !BUILDFLAG(IS_ANDROID)
  }

  std::vector<mojom::SkillPreviewPtr> GetSkillPreviewsList() {
    std::vector<mojom::SkillPreviewPtr> skill_previews;
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)
    if (!skills_service_) {
      return skill_previews;
    }
    const std::vector<std::unique_ptr<skills::Skill>>& skills =
        skills_service_->GetSkills();
    skill_previews.reserve(skills.size());
    for (const auto& skill : skills) {
      skill_previews.push_back(ToMojomSkillPreview(skill.get()));
    }
#endif  //  !BUILDFLAG(IS_ANDROID)
    return skill_previews;
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
  std::unique_ptr<JournalHandler> journal_handler_;
  std::unique_ptr<DebouncerDeduper> debouncer_deduper_;
  std::unique_ptr<PageMetadataManager> page_metadata_manager_;
// NEEDS_ANDROID_IMPL: (crbug.com/477622144) Remove desktop-only restrictions
// from Skills backend.
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  raw_ptr<skills::SkillsService> skills_service_;
#endif
  base::WeakPtr<actor::AutofillSelectionDialogEventHandler>
      autofill_selection_event_handler_;
  bool floating_panel_can_attach_ = false;
};

GlicPageHandler::GlicPageHandler(
    content::WebContents* webui_contents,
    Host* host,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page)
    : host_(*host),
      webui_contents_(webui_contents),
      browser_context_(webui_contents->GetBrowserContext()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  GetGlicService()->host_manager().WebUIPageHandlerAdded(this, &host_.get());
  host_->AddPanelStateObserver(this);
  UpdatePageState(host_->GetPanelState(web_client_handler_.get()).kind);
  if (!base::FeatureList::IsEnabled(features::kGlicWebContentsWarming)) {
    subscriptions_.push_back(
        GetGlicService()->enabling().RegisterProfileReadyStateChanged(
            base::BindRepeating(&GlicPageHandler::UpdateProfileReadyState,
                                base::Unretained(this))));
    UpdateProfileReadyState();
  }
}

GlicPageHandler::~GlicPageHandler() {
  host_->RemovePanelStateObserver(this);
  WebUiStateChanged(glic::mojom::WebUiState::kUninitialized);
  // `GlicWebClientHandler` holds a pointer back to us, so delete it first.
  web_client_handler_.reset();
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

void GlicPageHandler::Zoom(mojom::ZoomAction zoom_action) {
  page_->Zoom(zoom_action);
}

content::RenderFrameHost* GlicPageHandler::GetGuestMainFrame() {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
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
  // TODO(b/470059315): Important to implement in Android.
  return nullptr;
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
  NavigateParams params(Profile::FromBrowserContext(browser_context_),
                        disabled_by_admin_link_url,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  DoNavigate(&params);
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

void GlicPageHandler::GetInternalsDataPayload(
    GetInternalsDataPayloadCallback callback) {
  mojom::InternalsDataPayloadPtr payload = mojom::InternalsDataPayload::New();

  payload->enablement = BuildProfileEnablement(
      browser_context_, GetGlicService()->actor_policy_checker());

  mojom::ConfigInfoPtr config = mojom::ConfigInfo::New();
  config->guest_url = GetGuestURL();
  config->fre_guest_url =
      GetFreURL(Profile::FromBrowserContext(browser_context_));

  config->autopush_guest_url = GURL(g_browser_process->local_state()->GetString(
      prefs::kGlicGuestUrlPresetAutopush));
  config->staging_guest_url = GURL(g_browser_process->local_state()->GetString(
      prefs::kGlicGuestUrlPresetStaging));
  config->preprod_guest_url = GURL(g_browser_process->local_state()->GetString(
      prefs::kGlicGuestUrlPresetPreprod));
  config->prod_guest_url = GURL(g_browser_process->local_state()->GetString(
      prefs::kGlicGuestUrlPresetProd));

  payload->config = std::move(config);

  std::move(callback).Run(std::move(payload));
}

void GlicPageHandler::SetGuestUrlPresets(const GURL& autopush_url,
                                         const GURL& staging_url,
                                         const GURL& preprod_url,
                                         const GURL& prod_url) {
  g_browser_process->local_state()->SetString(
      prefs::kGlicGuestUrlPresetAutopush, autopush_url.spec());
  g_browser_process->local_state()->SetString(prefs::kGlicGuestUrlPresetStaging,
                                              staging_url.spec());
  g_browser_process->local_state()->SetString(prefs::kGlicGuestUrlPresetPreprod,
                                              preprod_url.spec());
  g_browser_process->local_state()->SetString(prefs::kGlicGuestUrlPresetProd,
                                              prod_url.spec());
}

void GlicPageHandler::PanelStateChanged(
    const glic::mojom::PanelState& panel_state,
    const PanelStateContext& context) {
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
