// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

bool CheckFreStatus(Profile* profile, prefs::FreStatus status) {
  return profile->GetPrefs()->GetInteger(prefs::kGlicCompletedFre) ==
         static_cast<int>(status);
}

class DummyDelegateImpl : public GlicMetrics::Delegate {
 public:
  gfx::Size GetWindowSize() const override { return {}; }
  bool IsWindowShowing() const override { return false; }
  bool IsWindowAttached() const override { return false; }
  content::WebContents* GetFocusedWebContents() override { return nullptr; }
  ActiveTabSharingState GetActiveTabSharingState() override {
    return ActiveTabSharingState::kNoTabCanBeShared;
  }
  int32_t GetNumPinnedTabs() const override { return 0; }
  std::vector<content::WebContents*> GetPinnedAndSharedWebContents() override {
    return std::vector<content::WebContents*>();
  }
};

class BaseDelegate : public GlicMetrics::Delegate {
 public:
  explicit BaseDelegate(GlicSharingManager* sharing_manager,
                        PrefService* pref_service)
      : sharing_manager_(sharing_manager), pref_service_(pref_service) {}
  content::WebContents* GetFocusedWebContents() override {
    FocusedTabData ftd = sharing_manager_->GetFocusedTabData();
    return ftd.is_focus() ? ftd.focus()->GetContents() : nullptr;
  }
  ActiveTabSharingState GetActiveTabSharingState() override {
    if (!pref_service_->GetBoolean(prefs::kGlicTabContextEnabled)) {
      return ActiveTabSharingState::kTabContextPermissionNotGranted;
    }
    FocusedTabData ftd = sharing_manager_->GetFocusedTabData();
    if (ftd.is_focus()) {
      return ActiveTabSharingState::kActiveTabIsShared;
    } else if (ftd.unfocused_tab()) {
      return ActiveTabSharingState::kCannotShareActiveTab;
    }
    return ActiveTabSharingState::kNoTabCanBeShared;
  }
  int32_t GetNumPinnedTabs() const override {
    return sharing_manager_->GetNumPinnedTabs();
  }
  std::vector<content::WebContents*> GetPinnedAndSharedWebContents() override {
    std::vector<content::WebContents*> pinned_and_shared;
    for (content::WebContents* web_contents :
         sharing_manager_->GetPinnedTabs()) {
      if (IsTabValidForSharing(web_contents)) {
        pinned_and_shared.push_back(web_contents);
      }
    }
    return pinned_and_shared;
  }

 protected:
  raw_ptr<GlicSharingManager> sharing_manager_;
  raw_ptr<PrefService> pref_service_;
};

class DelegateImpl : public BaseDelegate {
 public:
  explicit DelegateImpl(GlicWindowControllerInterface* window_controller,
                        GlicSharingManager* sharing_manager,
                        PrefService* pref_service)
      : BaseDelegate(sharing_manager, pref_service),
        window_controller_(window_controller) {}
  gfx::Size GetWindowSize() const override {
    return window_controller_->GetPanelSize();
  }
  bool IsWindowShowing() const override {
    return window_controller_->IsShowing();
  }
  bool IsWindowAttached() const override {
    return window_controller_->IsAttached();
  }

 private:
  raw_ptr<GlicWindowControllerInterface> window_controller_;
};

class DelegateMultiInstanceImpl : public BaseDelegate {
 public:
  explicit DelegateMultiInstanceImpl(GlicInstance* glic_instance,
                                     GlicSharingManager* sharing_manager,
                                     PrefService* pref_service)
      : BaseDelegate(sharing_manager, pref_service),
        glic_instance_(glic_instance) {}
  gfx::Size GetWindowSize() const override {
    return glic_instance_->GetPanelSize();
  }
  bool IsWindowShowing() const override { return glic_instance_->IsShowing(); }
  bool IsWindowAttached() const override {
    return glic_instance_->IsAttached();
  }

 private:
  raw_ptr<GlicInstance> glic_instance_;
};

constexpr char kHistogramGlicPanelPresentationTimePrefix[] =
    "Glic.PanelPresentationTime2.";

constexpr static base::TimeDelta kLogSizeMetricsDelay = base::Minutes(3);

enum class ModeOffset : int {
  kTextAttached = 1,
  kAudioAttached = 2,
  kTextDetached = 3,
  kAudioDetached = 4,
  kMaxValue = kAudioDetached,
};

ResponseSegmentation GetResponseSegmentation(bool attached,
                                             mojom::WebClientMode mode,
                                             mojom::InvocationSource source) {
  if (mode == mojom::WebClientMode::kUnknown) {
    return ResponseSegmentation::kUnknown;
  }

  ModeOffset modeOffset;
  if (mode == mojom::WebClientMode::kText && attached) {
    modeOffset = ModeOffset::kTextAttached;
  } else if (mode == mojom::WebClientMode::kAudio && attached) {
    modeOffset = ModeOffset::kAudioAttached;
  } else if (mode == mojom::WebClientMode::kText && !attached) {
    modeOffset = ModeOffset::kTextDetached;
  } else {
    modeOffset = ModeOffset::kAudioDetached;
  }

  int baseIndex =
      static_cast<int>(source) * (static_cast<int>(ModeOffset::kMaxValue));
  int offset = static_cast<int>(modeOffset);

  return static_cast<ResponseSegmentation>(baseIndex + offset);
}

std::string_view GetInputModeString(mojom::WebClientMode input_mode) {
  switch (input_mode) {
    case mojom::WebClientMode::kText:
      return "Text";
    case mojom::WebClientMode::kAudio:
      return "Audio";
    case mojom::WebClientMode::kUnknown:
      return "Unknown";
  }
}

}  // namespace

namespace internal {

// LINT.IfChange(BrowserActiveState)
// This must match enums.xml.
enum class BrowserActiveState {
  // A browser window is currently active, or was active less than one second
  // ago. This 1 second allowance helps ignore differences in window activation
  // timing for different platforms.
  kBrowserActive = 0,
  // A browser window is not active, but was active within the last N seconds,
  // and is still visible.
  kBrowserRecentlyActive1to5s = 1,
  kBrowserRecentlyActive5to10s = 2,
  kBrowserRecentlyActive10to30s = 3,
  // No browser windows are active or have been active within the last 10
  // seconds, but a browser window is still visible.
  kBrowserInactive = 4,
  // No browser windows are visible.
  kBrowserHidden = 5,

  kMaxValue = kBrowserHidden,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicBrowserActiveState)

// Computes BrowserActiveState.
class BrowserActivityObserver : public BrowserListObserver {
 public:
  BrowserActivityObserver() { BrowserList::AddObserver(this); }
  ~BrowserActivityObserver() override { BrowserList::RemoveObserver(this); }

  BrowserActiveState GetBrowserActiveState() const {
    if (active_browser_) {
      return BrowserActiveState::kBrowserActive;
    }
    bool browser_hidden = true;
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&browser_hidden](BrowserWindowInterface* browser_window_interface) {
          if (!browser_window_interface->GetWindow()->IsMinimized() &&
              browser_window_interface->capabilities()->IsVisibleOnScreen() &&
              browser_window_interface->GetWindow()->IsVisible()) {
            browser_hidden = false;
            return false;
          }
          return true;
        });
    if (browser_hidden) {
      return BrowserActiveState::kBrowserHidden;
    }
    if (last_browser_active_time_) {
      auto time_since_active =
          base::TimeTicks::Now() - *last_browser_active_time_;
      if (time_since_active < base::Seconds(1)) {
        return BrowserActiveState::kBrowserActive;
      } else if (time_since_active < base::Seconds(5)) {
        return BrowserActiveState::kBrowserRecentlyActive1to5s;
      } else if (time_since_active < base::Seconds(10)) {
        return BrowserActiveState::kBrowserRecentlyActive5to10s;
      } else if (time_since_active < base::Seconds(30)) {
        return BrowserActiveState::kBrowserRecentlyActive10to30s;
      }
    }
    return BrowserActiveState::kBrowserInactive;
  }

  // BrowserListObserver impl.
  void OnBrowserRemoved(Browser* browser) override {
    if (active_browser_ == browser) {
      active_browser_ = nullptr;
    }
  }
  void OnBrowserSetLastActive(Browser* browser) override {
    active_browser_ = browser;
    last_browser_active_time_ = std::nullopt;
  }
  void OnBrowserNoLongerActive(Browser* browser) override {
    if (active_browser_ == browser) {
      active_browser_ = nullptr;
    }
    if (!active_browser_) {
      last_browser_active_time_ = base::TimeTicks::Now();
    }
  }

 private:
  // The active browser, or null if none is active.
  raw_ptr<Browser> active_browser_ = nullptr;

  // If the browser is not active, the time at which it was last active.
  std::optional<base::TimeTicks> last_browser_active_time_;
};

}  // namespace internal

GlicMetrics::GlicMetrics(Profile* profile, GlicEnabling* enabling)
    : profile_(profile),
      enabling_(enabling),
      browser_activity_observer_(
          std::make_unique<internal::BrowserActivityObserver>()) {
  impression_timer_.Start(
      FROM_HERE, base::Minutes(15),
      base::BindRepeating(&GlicMetrics::OnImpressionTimerFired,
                          base::Unretained(this)));

  subscriptions_.push_back(
      enabling_->RegisterAllowedChanged(base::BindRepeating(
          &GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged,
          base::Unretained(this))));

  is_enabled_ = enabling_->IsEnabledAndConsentForProfile(profile_);
  is_pinned_ = profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kGlicCompletedFre,
      base::BindRepeating(
          &GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged,
          base::Unretained(this)));
  pref_registrar_.Add(prefs::kGlicPinnedToTabstrip,
                      base::BindRepeating(&GlicMetrics::OnPinningPrefChanged,
                                          base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kGlicTabContextEnabled,
      base::BindRepeating(&GlicMetrics::OnTabContextEnabledPrefChanged,
                          base::Unretained(this)));
}

GlicMetrics::~GlicMetrics() = default;

void GlicMetrics::OnTrustFirstOnboardingShown() {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Shown"));
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Shown.Onboarding"));
  onboarding_shown_time_ = base::TimeTicks::Now();
}

void GlicMetrics::OnTrustFirstOnboardingAccept() {
  OnFreAccepted();
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Accept"));
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Accept.Onboarding"));

  if (!onboarding_shown_time_.is_null()) {
    base::UmaHistogramLongTimes(
        "Glic.Fre.TotalTime.Accepted.Onboarding",
        base::TimeTicks::Now() - onboarding_shown_time_);
    onboarding_shown_time_ = base::TimeTicks();
  }
}

void GlicMetrics::OnTrustFirstOnboardingDismissed() {
  if (onboarding_shown_time_.is_null() ||
      enabling_->HasConsentedForProfile(profile_)) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Dismissed.Onboarding"));

  base::UmaHistogramLongTimes("Glic.Fre.TotalTime.Dismissed.Onboarding",
                              base::TimeTicks::Now() - onboarding_shown_time_);
  onboarding_shown_time_ = base::TimeTicks();
}

void GlicMetrics::OnFreAccepted() {
  // Store the current time in a instance variable.
  fre_accepted_time_ = base::TimeTicks::Now();
}

void GlicMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  if (!fre_accepted_time_.is_null()) {
    base::TimeDelta delta = base::TimeTicks::Now() - fre_accepted_time_;
    base::UmaHistogramLongTimes("Glic.FreToFirstQueryTime", delta);
    base::UmaHistogramCustomTimes("Glic.FreToFirstQueryTimeMax24H", delta,
                                  base::Milliseconds(1), base::Hours(24), 50);
    fre_accepted_time_ = base::TimeTicks();
  }
  base::UmaHistogramEnumeration(
      "Glic.Session.InputSubmit.BrowserActiveState",
      browser_activity_observer_->GetBrowserActiveState());
  base::RecordAction(base::UserMetricsAction("GlicResponseInputSubmit"));
  base::UmaHistogramEnumeration(
      "Glic.Sharing.ActiveTabSharingState.OnUserInputSubmitted",
      delegate_->GetActiveTabSharingState());
  // Reset turn data and start populating it for the new turn being started.
  turn_ = {};
  turn_.input_submitted_time_ = base::TimeTicks::Now();
  // Favor using the focused tab for UKM source; otherwise use the latest
  // extracted one if there are any pinned tabs being shared. If none of these
  // is true, leave turn_.chosen_source_id_ as its default of NoURLSourceId.
  content::WebContents* focused = delegate_->GetFocusedWebContents();
  if (focused) {
    turn_.chosen_source_id_ =
        focused->GetPrimaryMainFrame()->GetPageUkmSourceId();
  } else if (delegate_->GetPinnedAndSharedWebContents().size() > 0) {
    turn_.chosen_source_id_ = last_tab_context_source_id_;
  }
  last_tab_context_source_id_ = ukm::NoURLSourceId();

  input_mode_ = mode;
  inputs_modes_used_.insert(mode);
}

void GlicMetrics::OnContextUploadStarted() {
  last_upload_start_time_ = base::TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction("GlicContextUploadStarted"));
}

void GlicMetrics::OnContextUploadCompleted() {
  if (last_upload_start_time_) {
    base::UmaHistogramMediumTimes(
        "Glic.TabContext.UploadTime",
        base::TimeTicks::Now() - *last_upload_start_time_);
    last_upload_start_time_ = std::nullopt;
  }
  base::RecordAction(base::UserMetricsAction("GlicContextUploadCompleted"));
}

void GlicMetrics::OnReaction(mojom::MetricUserInputReactionType reaction_type) {
  std::optional<base::TimeDelta> time_to_reaction;
  if (!turn_.input_submitted_time_.is_null() &&
      input_mode_ == mojom::WebClientMode::kText) {
    time_to_reaction = base::TimeTicks::Now() - turn_.input_submitted_time_;
  }

  switch (reaction_type) {
    case mojom::MetricUserInputReactionType::kUnknown:
      base::RecordAction(base::UserMetricsAction("GlicReactionUnknown"));
      return;
    case mojom::MetricUserInputReactionType::kCanned:
      base::RecordAction(base::UserMetricsAction("GlicReactionCanned"));
      if (time_to_reaction && !turn_.reported_reaction_time_canned_) {
        base::UmaHistogramMediumTimes("Glic.FirstReaction.Text.Canned.Time",
                                      *time_to_reaction);
        turn_.reported_reaction_time_canned_ = true;
      }
      return;
    case mojom::MetricUserInputReactionType::kModel:
      base::RecordAction(base::UserMetricsAction("GlicReactionModelled"));
      if (time_to_reaction && !turn_.reported_reaction_time_modelled_) {
        base::UmaHistogramMediumTimes("Glic.FirstReaction.Text.Modelled.Time",
                                      *time_to_reaction);
        turn_.reported_reaction_time_modelled_ = true;
      }
      return;
  }
}

void GlicMetrics::OnResponseStarted() {
  turn_.response_started_ = true;
  base::UmaHistogramEnumeration(
      "Glic.Session.ResponseStart.BrowserActiveState",
      browser_activity_observer_->GetBrowserActiveState());
  base::RecordAction(base::UserMetricsAction("GlicResponseStart"));

  // It doesn't make sense to record response start without input submission.
  if (turn_.input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStartWithoutInput);
    return;
  }

  if (!delegate_->IsWindowShowing()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStartWhileHidingOrHidden);
    return;
  }

  base::TimeDelta start_time =
      base::TimeTicks::Now() - turn_.input_submitted_time_;
  base::UmaHistogramMediumTimes("Glic.Response.StartTime", start_time);
  std::string_view mode_string = GetInputModeString(input_mode_);
  base::UmaHistogramMediumTimes(
      base::StrCat({"Glic.Response.StartTime.InputMode.", mode_string}),
      start_time);

  // If source ID was chosen, we assume tab context was extracted.
  if (turn_.chosen_source_id_ != ukm::NoURLSourceId()) {
    base::UmaHistogramMediumTimes(
        "Glic.Response.StartTime.TabContext.LikelyWith", start_time);
  } else {
    base::UmaHistogramMediumTimes(
        "Glic.Response.StartTime.TabContext.LikelyWithout", start_time);
  }
  base::RecordAction(base::UserMetricsAction("GlicResponse"));
  ++session_responses_;

  // More detailed metrics.
  bool attached = delegate_->IsWindowAttached();
  base::UmaHistogramBoolean("Glic.Response.Attached", attached);
  base::UmaHistogramEnumeration("Glic.Response.InvocationSource",
                                invocation_source_);
  base::UmaHistogramEnumeration("Glic.Response.InputMode", input_mode_);
  base::UmaHistogramEnumeration(
      "Glic.Response.Segmentation",
      GetResponseSegmentation(attached, input_mode_, invocation_source_));

  base::UmaHistogramCounts100("Glic.Response.TabsPinnedForSharingCount",
                              delegate_->GetNumPinnedTabs());

  ukm::builders::Glic_Response(turn_.chosen_source_id_)
      .SetAttached(attached)
      .SetInvocationSource(static_cast<int64_t>(invocation_source_))
      .SetWebClientMode(static_cast<int64_t>(input_mode_))
      .Record(ukm::UkmRecorder::Get());
}

void GlicMetrics::OnResponseStopped(mojom::ResponseStopCause cause) {
  // The client may call "stopped" without "started" for very short responses.
  // We synthetically call it ourselves in this case.
  if (!turn_.input_submitted_time_.is_null() && !turn_.response_started_) {
    OnResponseStarted();
  }

  base::RecordAction(base::UserMetricsAction("GlicResponseStop"));
  std::string_view cause_suffix;
  switch (cause) {
    case mojom::ResponseStopCause::kUser:
      cause_suffix = ".ByUser";
      base::RecordAction(base::UserMetricsAction("GlicResponseStopByUser"));
      break;
    case mojom::ResponseStopCause::kOther:
      cause_suffix = ".Other";
      base::RecordAction(base::UserMetricsAction("GlicResponseStopOther"));
      break;
    case mojom::ResponseStopCause::kUnknown:
      cause_suffix = ".UnknownCause";
      base::RecordAction(
          base::UserMetricsAction("GlicResponseStopUnknownCause"));
      break;
  }

  if (turn_.input_submitted_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kResponseStopWithoutInput);
    base::UmaHistogramEnumeration(
        base::StrCat({"Glic.Metrics.Error", cause_suffix}),
        Error::kResponseStopWithoutInput);
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    base::UmaHistogramMediumTimes("Glic.Response.StopTime",
                                  now - turn_.input_submitted_time_);
    base::UmaHistogramMediumTimes(
        base::StrCat({"Glic.Response.StopTime", cause_suffix}),
        now - turn_.input_submitted_time_);
  }

  // Reset the turn.
  turn_ = {};
}

void GlicMetrics::OnSessionTerminated() {
  base::RecordAction(base::UserMetricsAction("GlicWebClientSessionEnd"));
}

void GlicMetrics::OnResponseRated(bool positive) {
  base::UmaHistogramBoolean("Glic.Response.Rated", positive);
}

void GlicMetrics::OnTurnCompleted(mojom::WebClientModel model,
                                  base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(model == mojom::WebClientModel::kActor
                                    ? "Glic.Response.TurnDuration.Actor"
                                    : "Glic.Response.TurnDuration.Default",
                                duration);
}

void GlicMetrics::OnModelChanged(mojom::WebClientModel model) {
  current_model_ = model;
}

void GlicMetrics::OnRecordUseCounter(uint16_t counter) {
  static_assert(1000u > static_cast<uint32_t>(mojom::WebUseCounter::kMaxValue));
  // Since the front end can contain a newer version than what chrome is
  // build against we use a sparse histogram.
  base::UmaHistogramSparse(
      "Glic.Api.UseCounter",
      std::clamp(static_cast<uint32_t>(counter), 0u, 1000u));
}

void GlicMetrics::OnGlicWindowStartedOpening(bool attached,
                                             mojom::InvocationSource source) {
  if (GlicEnabling::IsTrustFirstOnboardingEnabled() &&
      !enabling_->HasConsentedForProfile(profile_)) {
    OnTrustFirstOnboardingShown();
  }

  base::UmaHistogramEnumeration(
      "Glic.Session.Open.BrowserActiveState",
      browser_activity_observer_->GetBrowserActiveState());
  base::RecordAction(base::UserMetricsAction("GlicSessionBegin"));
  show_start_time_ = base::TimeTicks::Now();
  session_start_time_ = base::TimeTicks::Now();
  invocation_source_ = source;
  base::UmaHistogramBoolean("Glic.Session.Open.Attached", attached);
  base::UmaHistogramEnumeration("Glic.Session.Open.InvocationSource", source);

  // TODO(b/452120577): turn.chosen_source_id_ is still undefined at this point.
  ukm::builders::Glic_WindowOpen(turn_.chosen_source_id_)
      .SetAttached(attached)
      .SetInvocationSource(static_cast<int64_t>(source))
      .Record(ukm::UkmRecorder::Get());

  const base::Time last_dismissed_time =
      profile_->GetPrefs()->GetTime(prefs::kGlicWindowLastDismissedTime);
  if (!last_dismissed_time.is_null()) {
    base::TimeDelta elapsed_time_from_last_session =
        base::Time::Now() - last_dismissed_time;
    base::UmaHistogramCounts10M(
        "Glic.PanelWebUi.ElapsedTimeBetweenSessions",
        base::saturated_cast<int>(elapsed_time_from_last_session.InSeconds()));
  }

  // Update the last dismissed timestamp. The pref might not get updated on
  // ungraceful shutdowns. As such, by updating the pref on opening the Glic
  // window, the dismissal timestamp will get approximated by the opening
  // timestamp, instead of the previously dismissal timestamp.
  profile_->GetPrefs()->SetTime(prefs::kGlicWindowLastDismissedTime,
                                base::Time::Now());
}

void GlicMetrics::OnGlicWindowOpenInterrupted() {
  show_start_time_ = base::TimeTicks();
}

void GlicMetrics::OnGlicWindowOpenAndReady() {
  if (show_start_time_.is_null()) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Glic.Sharing.ActiveTabSharingState.OnPanelOpenAndReady",
      delegate_->GetActiveTabSharingState());

  // Record the presentation time of showing the glic panel in an UMA histogram.
  base::TimeDelta presentation_time = base::TimeTicks::Now() - show_start_time_;
  base::UmaHistogramCustomTimes(
      base::StrCat({kHistogramGlicPanelPresentationTimePrefix, "All"}),
      presentation_time, base::Milliseconds(1), base::Seconds(60), 50);
  if (input_mode_ != mojom::WebClientMode::kUnknown) {
    std::string_view mode_string = GetInputModeString(input_mode_);
    base::UmaHistogramCustomTimes(
        base::StrCat({kHistogramGlicPanelPresentationTimePrefix, mode_string}),
        presentation_time, base::Milliseconds(1), base::Seconds(60), 50);
  }

  OnGlicWindowOpenInterrupted();
}

void GlicMetrics::OnGlicWindowShown(
    Browser* browser,
    std::optional<display::Display> glic_display,
    const gfx::Rect& glic_bounds) {
  GlicMetrics::OnGlicWindowSizeTimerFired();
  glic_window_size_timer_.Start(
      FROM_HERE, kLogSizeMetricsDelay,
      base::BindRepeating(&GlicMetrics::OnGlicWindowSizeTimerFired,
                          base::Unretained(this)));
  base::UmaHistogramEnumeration(
      "Glic.PositionOnDisplay.OnOpen",
      GetDisplayPositionOfPoint(glic_display, glic_bounds.CenterPoint()));
  base::UmaHistogramEnumeration(
      "Glic.PositionOnChrome.OnOpen",
      GetChromeRelativePositionOfPoint(browser, glic_bounds.CenterPoint()));
  base::UmaHistogramEnumeration(
      "Glic.PercentOverlapWithBrowser.OnOpen",
      GetPercentOverlapWithBrowser(browser, glic_bounds));
}

void GlicMetrics::OnGlicWindowResize() {
  base::RecordAction(base::UserMetricsAction("GlicPanelResized"));
}

void GlicMetrics::OnWidgetUserResizeStarted() {
  base::RecordAction(base::UserMetricsAction("GlicPanelUserResizeStarted"));

  gfx::Size size_on_user_resize_started = delegate_->GetWindowSize();
  base::UmaHistogramCounts10000("Glic.PanelWebUi.UserResizeStarted.Width",
                                size_on_user_resize_started.width());
  base::UmaHistogramCounts10000("Glic.PanelWebUi.UserResizeStarted.Height",
                                size_on_user_resize_started.height());
}

void GlicMetrics::OnWidgetUserResizeEnded() {
  base::RecordAction(base::UserMetricsAction("GlicPanelUserResizeEnded"));

  gfx::Size size_on_user_resize_ended = delegate_->GetWindowSize();
  base::UmaHistogramCounts10000("Glic.PanelWebUi.UserResizeEnded.Width",
                                size_on_user_resize_ended.width());
  base::UmaHistogramCounts10000("Glic.PanelWebUi.UserResizeEnded.Height",
                                size_on_user_resize_ended.height());
}

void GlicMetrics::OnGlicWindowClose(Browser* last_active_browser,
                                    std::optional<display::Display> display,
                                    const gfx::Rect& glic_bounds) {
  base::RecordAction(base::UserMetricsAction("GlicSessionEnd"));
  base::UmaHistogramEnumeration(
      "Glic.PositionOnDisplay.OnClose",
      GetDisplayPositionOfPoint(display, glic_bounds.CenterPoint()));
  base::UmaHistogramEnumeration(
      "Glic.PositionOnChrome.OnClose",
      GetChromeRelativePositionOfPoint(last_active_browser,
                                       glic_bounds.CenterPoint()));
  base::UmaHistogramEnumeration(
      "Glic.PercentOverlapWithBrowser.OnClose",
      GetPercentOverlapWithBrowser(last_active_browser, glic_bounds));
  base::UmaHistogramCounts1000("Glic.Session.ResponseCount",
                               session_responses_);
  if (session_start_time_.is_null()) {
    base::UmaHistogramEnumeration("Glic.Metrics.Error",
                                  Error::kWindowCloseWithoutWindowOpen);
  } else {
    base::TimeDelta open_time = base::TimeTicks::Now() - session_start_time_;
    base::UmaHistogramCustomTimes("Glic.Session.Duration", open_time,
                                  /*min=*/base::Seconds(1),
                                  /*max=*/base::Days(10), /*buckets=*/50);
  }
  session_responses_ = 0;
  session_start_time_ = base::TimeTicks();

  InputModesUsed modes_used = InputModesUsed::kNone;
  if (!inputs_modes_used_.empty()) {
    if (inputs_modes_used_.size() == 2) {
      modes_used = InputModesUsed::kTextAndAudio;
    } else {
      modes_used = inputs_modes_used_.contains(mojom::WebClientMode::kAudio)
                       ? InputModesUsed::kOnlyAudio
                       : InputModesUsed::kOnlyText;
    }
  }
  inputs_modes_used_.clear();
  base::UmaHistogramEnumeration("Glic.Session.InputModesUsed", modes_used);

  base::UmaHistogramCounts100("Glic.Session.AttachStateChanges",
                              attach_change_count_);
  attach_change_count_ = 0;

  if (base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
    base::UmaHistogramCounts100("Glic.ScrollTo.SessionCount",
                                scroll_attempt_count_);
    scroll_attempt_count_ = 0;
  }

  if (base::FeatureList::IsEnabled(features::kGlicTrustFirstOnboarding)) {
    if (!onboarding_shown_time_.is_null() &&
        !enabling_->HasConsentedForProfile(profile_)) {
      OnTrustFirstOnboardingDismissed();
    }
    onboarding_shown_time_ = base::TimeTicks();
  }

  glic_window_size_timer_.Stop();
  profile_->GetPrefs()->SetTime(prefs::kGlicWindowLastDismissedTime,
                                base::Time::Now());
}

void GlicMetrics::OnGlicScrollAttempt() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicScrollTo));
  ++scroll_attempt_count_;
  if (!turn_.input_submitted_time_.is_null()) {
    scroll_input_submitted_time_ = turn_.input_submitted_time_;
    scroll_input_mode_ = input_mode_;
  }
}

void GlicMetrics::OnGlicScrollComplete(bool success) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicScrollTo));
  if (success && !scroll_input_submitted_time_.is_null()) {
    base::TimeDelta time_to_scroll =
        base::TimeTicks::Now() - scroll_input_submitted_time_;
    std::string_view mode_string = GetInputModeString(scroll_input_mode_);
    base::UmaHistogramMediumTimes(
        base::StrCat({"Glic.ScrollTo.UserPromptToScrollTime.", mode_string}),
        time_to_scroll);
  }
  scroll_input_submitted_time_ = base::TimeTicks();
  scroll_input_mode_ = mojom::WebClientMode::kUnknown;
}

void GlicMetrics::LogClosedCaptionsShown() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicClosedCaptioning));
  bool pref_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicClosedCaptioningEnabled);
  base::UmaHistogramBoolean("Glic.Response.ClosedCaptionsShown", pref_enabled);
}

void GlicMetrics::OnShareImageStarted() {
  share_image_start_time_ = base::TimeTicks::Now();
}

void GlicMetrics::OnShareImageComplete(ShareImageResult result) {
  if (!share_image_start_time_.is_null() &&
      result == ShareImageResult::kSuccess) {
    base::UmaHistogramMediumTimes(
        "Glic.TabContext.ShareImageDuration",
        base::TimeTicks::Now() - share_image_start_time_);
    share_image_start_time_ = base::TimeTicks();
  }
  base::UmaHistogramEnumeration("Glic.TabContext.ShareImageResult", result);
}

void GlicMetrics::LogGetContextFromFocusedTabError(
    GlicGetContextFromTabError error) {
  std::string_view mode_string = GetInputModeString(input_mode_);
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Api.GetContextFromFocusedTab.Error.", mode_string}),
      error);
}

void GlicMetrics::LogGetContextFromTabError(GlicGetContextFromTabError error) {
  std::string_view mode_string = GetInputModeString(input_mode_);
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Api.GetContextFromTab.Error.", mode_string}), error);
}

void GlicMetrics::LogGetContextForActorFromTabError(
    GlicGetContextFromTabError error) {
  std::string_view mode_string = GetInputModeString(input_mode_);
  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Api.GetContextForActorFromTab.Error.", mode_string}),
      error);
}

void GlicMetrics::OnActivateTabFromInstance(tabs::TabInterface* tab) {
  actor::TaskId task_id =
      actor::ActorKeyedService::Get(profile_)->GetTaskFromTab(*tab);
  // Record user action if the tab is associated with an ActorTask.
  if (!task_id.is_null()) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.TaskTabForegrounded"));
  }
}

void GlicMetrics::SetControllers(
    GlicWindowControllerInterface* window_controller,
    GlicSharingManager* sharing_manager) {
  delegate_ = std::make_unique<DelegateImpl>(window_controller, sharing_manager,
                                             profile_->GetPrefs());
}

void GlicMetrics::SetControllersWithInstance(
    GlicInstance* glic_instance,
    GlicSharingManager* sharing_manager) {
  delegate_ = std::make_unique<DelegateMultiInstanceImpl>(
      glic_instance, sharing_manager, profile_->GetPrefs());
}
void GlicMetrics::ClearControllers() {
  delegate_ = std::make_unique<DummyDelegateImpl>();
}
void GlicMetrics::SetDelegateForTesting(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void GlicMetrics::DidRequestContextFromTab(content::WebContents& web_contents) {
  last_tab_context_source_id_ =
      web_contents.GetPrimaryMainFrame()->GetPageUkmSourceId();
}

void GlicMetrics::SetWebClientMode(mojom::WebClientMode mode) {
  input_mode_ = mode;
}

void GlicMetrics::OnImpressionTimerFired() {
  if (!enabling_->IsAllowed()) {
    EntryPointStatus impression;
    if (CheckFreStatus(profile_, prefs::FreStatus::kNotStarted)) {
      // Profile not eligible, and not started FRE
      impression = EntryPointStatus::kBeforeFreNotEligible;
    } else if (CheckFreStatus(profile_, prefs::FreStatus::kIncomplete)) {
      // Profile not eligible, started but not completed FRE
      impression = EntryPointStatus::kIncompleteFreNotEligible;
    } else {
      // Profile not eligible, completed FRE
      impression = EntryPointStatus::kAfterFreNotEligible;
    }
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status", impression);
    return;
  }

  // Profile eligible, has not started FRE
  if (CheckFreStatus(profile_, prefs::FreStatus::kNotStarted)) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status",
                                  EntryPointStatus::kBeforeFreAndEligible);
    return;
  }

  // Profile eligible, started but not completed FRE
  if (CheckFreStatus(profile_, prefs::FreStatus::kIncomplete)) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status",
                                  EntryPointStatus::kIncompleteFreAndEligible);
    return;
  }

  // Profile eligible and completed FRE
  EntryPointStatus impression;
  bool is_os_entrypoint_enabled =
      g_browser_process->local_state()->GetBoolean(prefs::kGlicLauncherEnabled);
  if (is_pinned_ && is_os_entrypoint_enabled) {
    impression = EntryPointStatus::kAfterFreBrowserAndOs;
  } else if (is_pinned_) {
    impression = EntryPointStatus::kAfterFreBrowserOnly;
  } else if (is_os_entrypoint_enabled) {
    impression = EntryPointStatus::kAfterFreOsOnly;
  } else {
    impression = EntryPointStatus::kAfterFreThreeDotOnly;
  }
  base::UmaHistogramEnumeration("Glic.EntryPoint.Status", impression);

  ui::Accelerator saved_hotkey =
      glic::GlicLauncherConfiguration::GetGlobalHotkey();
  base::UmaHistogramBoolean("Glic.OsEntrypoint.Settings.ShortcutStatus",
                            saved_hotkey != ui::Accelerator());
}

void GlicMetrics::OnGlicWindowSizeTimerFired() {
  // A 4K screen is 3840 or 4096 pixels wide and 2160 tall. Doubling this and
  // rounding up to 10000 should give a reasonable upper bound on DIPs for
  // both directions.
  gfx::Size currentSize = delegate_->GetWindowSize();
  base::UmaHistogramCounts10000("Glic.PanelWebUi.Size.Width",
                                currentSize.width());
  base::UmaHistogramCounts10000("Glic.PanelWebUi.Size.Height",
                                currentSize.height());
  base::UmaHistogramCounts10M("Glic.PanelWebUi.Size.Area",
                              currentSize.GetArea());
}

void GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged() {
  bool is_enabled = enabling_->IsEnabledAndConsentForProfile(profile_);
  if (is_enabled == is_enabled_) {
    // No change, early exit.
    return;
  }
  is_enabled_ = is_enabled;
  if (is_enabled_) {
    base::RecordAction(base::UserMetricsAction("Glic.Enabled"));
  } else {
    base::RecordAction(base::UserMetricsAction("Glic.Disabled"));
  }
}

void GlicMetrics::OnPinningPrefChanged() {
  bool is_pinned =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  if (is_pinned == is_pinned_) {
    // No change, early exit.
    return;
  }
  is_pinned_ = is_pinned;
  if (is_pinned_) {
    base::RecordAction(base::UserMetricsAction("Glic.Pinned"));
  } else {
    base::RecordAction(base::UserMetricsAction("Glic.Unpinned"));
  }
}

void GlicMetrics::OnTabPinnedForSharing(GlicTabPinnedForSharingResult result) {
  base::UmaHistogramEnumeration("Glic.Sharing.TabPinnedForSharing", result);
}

void GlicMetrics::OnTabContextEnabledPrefChanged() {
  bool is_panel_open = !session_start_time_.is_null();
  bool is_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicTabContextEnabled);
  if (is_panel_open && is_enabled) {
    base::UmaHistogramEnumeration(
        "Glic.Sharing.ActiveTabSharingState."
        "OnTabContextPermissionGranted",
        delegate_->GetActiveTabSharingState());
  }
}

DisplayPosition GlicMetrics::GetDisplayPositionOfPoint(
    std::optional<display::Display> display,
    const gfx::Point& glic_center_point) {
  if (!display) {
    return DisplayPosition::kUnknown;
  }
  gfx::Rect work_area_bounds = display->work_area();
  if (!work_area_bounds.Contains(glic_center_point) ||
      work_area_bounds.IsEmpty()) {
    return DisplayPosition::kUnknown;
  }
  // Adjust glic center point to the origin of the display's work area.
  gfx::Point glic_work_area_center_point =
      glic_center_point - work_area_bounds.OffsetFromOrigin();
  int x_index = std::floor(3 * glic_work_area_center_point.x() /
                           work_area_bounds.width());
  int y_index = std::floor(3 * glic_work_area_center_point.y() /
                           work_area_bounds.height());

  // This is unexpected to happen but just in case.
  if (x_index < 0 || x_index > 2 || y_index < 0 || y_index > 2) {
    return DisplayPosition::kUnknown;
  }

  const std::array<std::array<DisplayPosition, 3>, 3> position_map = {{
      {DisplayPosition::kTopLeft, DisplayPosition::kCenterLeft,
       DisplayPosition::kBottomLeft},
      {DisplayPosition::kTopCenter, DisplayPosition::kCenterCenter,
       DisplayPosition::kBottomCenter},
      {DisplayPosition::kTopRight, DisplayPosition::kCenterRight,
       DisplayPosition::kBottomRight},
  }};
  return position_map[x_index][y_index];
}

ChromeRelativePosition GlicMetrics::GetChromeRelativePositionOfPoint(
    Browser* browser,
    const gfx::Point& glic_center_point) {
  if (!IsBrowserVisible(browser)) {
    return ChromeRelativePosition::kNoVisibleChromeBrowser;
  }

  // Check if the center point is on a different display
  std::optional<display::Display> browser_display =
      browser->GetBrowserView().GetWidget()->GetNearestDisplay();
  if (browser_display &&
      !browser_display->work_area().Contains(glic_center_point)) {
    return ChromeRelativePosition::kChromeOnOtherDisplay;
  }

  gfx::Rect browser_bounds =
      browser->GetBrowserView().GetWidget()->GetWindowBoundsInScreen();
  int x_index;
  if (glic_center_point.x() < browser_bounds.x()) {
    x_index = 0;
  } else if (glic_center_point.x() < browser_bounds.right()) {
    x_index = 1;
  } else {
    x_index = 2;
  }
  int y_index;
  if (glic_center_point.y() < browser_bounds.y()) {
    y_index = 0;
  } else if (glic_center_point.y() < browser_bounds.bottom()) {
    y_index = 1;
  } else {
    y_index = 2;
  }

  const std::array<std::array<ChromeRelativePosition, 3>, 3> position_map = {{
      {ChromeRelativePosition::kAboveLeft, ChromeRelativePosition::kCenterLeft,
       ChromeRelativePosition::kBelowLeft},
      {ChromeRelativePosition::kAboveCenter, ChromeRelativePosition::kOverlap,
       ChromeRelativePosition::kBelowCenter},
      {ChromeRelativePosition::kAboveRight,
       ChromeRelativePosition::kCenterRight,
       ChromeRelativePosition::kBelowRight},
  }};
  return position_map[x_index][y_index];
}

PercentOverlap GlicMetrics::GetPercentOverlapWithBrowser(
    Browser* browser,
    const gfx::Rect& glic_bounds) {
  if (!IsBrowserVisible(browser)) {
    return PercentOverlap::kNoVisibleChromeBrowser;
  }
  int glic_area = glic_bounds.width() * glic_bounds.height();
  if (glic_area == 0) {
    return PercentOverlap::k0;
  }
  gfx::Rect browser_glic_intersect_bounds =
      browser->GetBrowserView().GetWidget()->GetWindowBoundsInScreen();
  browser_glic_intersect_bounds.Intersect(glic_bounds);
  int browser_glic_intersect_area = browser_glic_intersect_bounds.width() *
                                    browser_glic_intersect_bounds.height();
  // Calculate overlap percentage and round to the nearest 10.
  int percentOverlap = round(10 * browser_glic_intersect_area / glic_area) * 10;
  switch (percentOverlap) {
    case 100:
      return PercentOverlap::k100;
    case 90:
      return PercentOverlap::k90;
    case 80:
      return PercentOverlap::k80;
    case 70:
      return PercentOverlap::k70;
    case 60:
      return PercentOverlap::k60;
    case 50:
      return PercentOverlap::k50;
    case 40:
      return PercentOverlap::k40;
    case 30:
      return PercentOverlap::k30;
    case 20:
      return PercentOverlap::k20;
    case 10:
      return PercentOverlap::k10;
    case 0:
    default:
      return PercentOverlap::k0;
  }
}

void GlicMetrics::OnAttachedToBrowser(AttachChangeReason reason) {
  base::UmaHistogramEnumeration("Glic.AttachedToBrowser", reason);
  if (reason != AttachChangeReason::kInit) {
    attach_change_count_++;
  }
}

void GlicMetrics::OnDetachedFromBrowser(AttachChangeReason reason) {
  base::UmaHistogramEnumeration("Glic.DetachedFromBrowser", reason);
  if (reason != AttachChangeReason::kInit) {
    attach_change_count_++;
  }
}

}  // namespace glic
