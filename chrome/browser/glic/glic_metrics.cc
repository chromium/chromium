// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/metrics/profile_metrics_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/chrome_features.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/base_window.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/widget/browser_conditions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/widget/widget.h"
#endif

namespace glic {

namespace {

bool CheckFreStatus(GlicEnabling* enabling, prefs::FreStatus status) {
  return enabling->GetCompletedFre() == status;
}

class DummyDelegateImpl : public GlicMetrics::Delegate {
 public:
  gfx::Size GetWindowSize() const override { return {}; }
  bool IsWindowShowing() const override { return false; }
  bool IsWindowAttached() const override { return false; }
  content::WebContents* GetFocusedWebContents() override { return nullptr; }
  int32_t GetNumPinnedTabs() const override { return 0; }
  std::vector<content::WebContents*> GetPinnedAndSharedWebContents() override {
    return std::vector<content::WebContents*>();
  }
};

class BaseDelegate : public GlicMetrics::Delegate {
 public:
  explicit BaseDelegate(GlicSharingManagerInternal* sharing_manager,
                        PrefService* pref_service)
      : sharing_manager_(sharing_manager), pref_service_(pref_service) {}
  content::WebContents* GetFocusedWebContents() override {
    FocusedTabData ftd = sharing_manager_->GetFocusedTabData();
    return ftd.is_focus() ? ftd.focus()->GetContents() : nullptr;
  }
  int32_t GetNumPinnedTabs() const override {
    return sharing_manager_->GetNumPinnedTabs();
  }
  std::vector<content::WebContents*> GetPinnedAndSharedWebContents() override {
    std::vector<content::WebContents*> pinned_and_shared;
    for (tabs::TabInterface* tab : sharing_manager_->GetPinnedTabs()) {
      content::WebContents* web_contents = tab->GetContents();
      if (web_contents && IsTabValidForSharing(web_contents)) {
        pinned_and_shared.push_back(web_contents);
      }
    }
    return pinned_and_shared;
  }

 protected:
  raw_ptr<GlicSharingManagerInternal> sharing_manager_;
  raw_ptr<PrefService> pref_service_;
};

class DelegateMultiInstanceImpl : public BaseDelegate {
 public:
  explicit DelegateMultiInstanceImpl(
      GlicInstance* glic_instance,
      GlicSharingManagerInternal* sharing_manager,
      PrefService* pref_service)
      : BaseDelegate(sharing_manager, pref_service),
        glic_instance_(glic_instance) {}
  gfx::Size GetWindowSize() const override {
    return glic_instance_->GetPanelSize();
  }
  bool IsWindowShowing() const override {
    return static_cast<GlicInstanceImpl*>(glic_instance_)->HasActiveEmbedder();
  }
  bool IsWindowAttached() const override {
    return glic_instance_->GetPanelState().kind ==
           mojom::PanelStateKind::kAttached;
  }

 private:
  raw_ptr<GlicInstance> glic_instance_;
};

constexpr char kHistogramGlicPanelPresentationTimePrefix[] =
    "Glic.PanelPresentationTime2.";

constexpr static base::TimeDelta kLogSizeMetricsDelay = base::Minutes(3);

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
class BrowserActivityObserver : public BrowserCollectionObserver {
 public:
  BrowserActivityObserver() {
    browser_collection_observer_.Observe(
        GlobalBrowserCollection::GetInstance());
  }
  ~BrowserActivityObserver() override = default;

  BrowserActiveState GetBrowserActiveState() const {
    if (active_browser_) {
      return BrowserActiveState::kBrowserActive;
    }
    bool browser_hidden = true;
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&browser_hidden](BrowserWindowInterface* browser_window_interface) {
          if (!browser_window_interface->GetWindow()->IsMinimized() &&
#if !BUILDFLAG(IS_ANDROID)
              // BrowserActiveState is only used for metrics.
              // `IsVisibleOnScreen()` is not available on Android, and we're
              // not bothering to add it.
              browser_window_interface->capabilities()->IsVisibleOnScreen() &&
#endif
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

  // BrowserCollectionObserver impl.
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    if (active_browser_ == browser) {
      active_browser_ = nullptr;
    }
  }
  void OnBrowserActivated(BrowserWindowInterface* browser) override {
    active_browser_ = browser;
    last_browser_active_time_ = std::nullopt;
  }
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override {
    if (active_browser_ == browser) {
      active_browser_ = nullptr;
    }
    if (!active_browser_) {
      last_browser_active_time_ = base::TimeTicks::Now();
    }
  }

 private:
  // The active browser, or null if none is active.
  raw_ptr<BrowserWindowInterface> active_browser_ = nullptr;

  // If the browser is not active, the time at which it was last active.
  std::optional<base::TimeTicks> last_browser_active_time_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observer_{this};
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

  if (enabling_->IsAllowed()) {
    RecordStartupEnablement();
  }

  is_enabled_ = enabling_->IsEnabledAndConsentForProfile(profile_);
  subscriptions_.push_back(
      enabling_->RegisterOnConsentChanged(base::BindRepeating(
          &GlicMetrics::OnMaybeEnabledAndConsentForProfileChanged,
          base::Unretained(this))));
}

GlicMetrics::~GlicMetrics() = default;

void GlicMetrics::RecordGlicProfilePreferences() {
  PrefService* profile_prefs = profile_->GetPrefs();
  PrefService* local_state = g_browser_process->local_state();

  base::UmaHistogramBoolean(
      "Glic.Preferences.PinnedToTabstrip",
      profile_prefs->GetBoolean(prefs::kGlicPinnedToTabstrip));
  base::UmaHistogramBoolean(
      "Glic.Preferences.LauncherEnabled",
      local_state->GetBoolean(prefs::kGlicLauncherEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.KeepSidepanelOpenOnNewTabsEnabled",
      profile_prefs->GetBoolean(prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.GeolocationEnabled",
      profile_prefs->GetBoolean(prefs::kGlicGeolocationEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.MicrophoneEnabled",
      profile_prefs->GetBoolean(prefs::kGlicMicrophoneEnabled));
  base::UmaHistogramBoolean(
      "Glic.Preferences.DefaultTabContextEnabled",
      profile_prefs->GetBoolean(prefs::kGlicDefaultTabContextEnabled));
  base::UmaHistogramBoolean("Glic.Preferences.ActuationOnWeb",
                            enabling_->GetUserEnabledActuationOnWeb());
}

void GlicMetrics::OnTrustFirstOnboardingAccept() {
  OnFreAccepted();
  OnOptInAccepted(OptInFlow::kGlicFre);
  base::UmaHistogramEnumeration("Glic.Fre.Accept.InvocationSource",
                                invocation_source_);

  if (!onboarding_shown_time_.is_null()) {
    base::UmaHistogramLongTimes(
        "Glic.Fre.TotalTime.Accepted.Onboarding",
        base::TimeTicks::Now() - onboarding_shown_time_);
    onboarding_shown_time_ = base::TimeTicks();
  }
}

void GlicMetrics::OnInstanceOpened() {
  if (!onboarding_shown_time_.is_null()) {
    return;
  }

  if (!enabling_->HasConsented()) {
    OnOptInShown(OptInFlow::kGlicFre);
    base::UmaHistogramEnumeration("Glic.Fre.Shown.InvocationSource",
                                  invocation_source_);
    onboarding_shown_time_ = base::TimeTicks::Now();
  }
}

void GlicMetrics::OnInstanceClosed() {
  if (onboarding_shown_time_.is_null()) {
    return;
  }

  OnOptInDismissed(OptInFlow::kGlicFre);
  base::UmaHistogramEnumeration("Glic.Fre.Dismissed.InvocationSource",
                                invocation_source_);
  base::UmaHistogramLongTimes("Glic.Fre.TotalTime.Dismissed.Onboarding",
                              base::TimeTicks::Now() - onboarding_shown_time_);
  onboarding_shown_time_ = base::TimeTicks();
}

void GlicMetrics::OnFreAccepted() {
  // Store the current time in a instance variable.
  fre_accepted_time_ = base::TimeTicks::Now();
  onboarding_invocation_source_ = invocation_source_;
}

void GlicMetrics::OnOptInShown(OptInFlow flow) {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Shown"));
  base::UmaHistogramEnumeration("Glic.Fre.Shown.FlowSource", flow);
}

void GlicMetrics::OnOptInImpression(OptInFlow flow) {
  base::RecordAction(
      base::UserMetricsAction("Glic.Onboarding.OptInImpression"));
  base::UmaHistogramEnumeration("Glic.Onboarding.OptInImpression.FlowSource",
                                flow);
}

void GlicMetrics::OnOptInAccepted(OptInFlow flow) {
  if (base::FeatureList::IsEnabled(features::kGlicOnboardingMetricsMigration)) {
    base::RecordAction(base::UserMetricsAction("Glic.Onboarding.OptInAccept"));
  } else {
    base::RecordAction(base::UserMetricsAction("Glic.Fre.Accept"));
  }
  base::UmaHistogramEnumeration("Glic.Fre.Accept.FlowSource", flow);
}

void GlicMetrics::OnOptInDismissed(OptInFlow flow) {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.Dismissed"));
  base::UmaHistogramEnumeration("Glic.Fre.Dismissed.FlowSource", flow);
}

void GlicMetrics::OnOptInRejected(OptInFlow flow) {
  base::RecordAction(base::UserMetricsAction("Glic.Fre.NoThanks"));
  base::UmaHistogramEnumeration("Glic.Fre.NoThanks.FlowSource", flow);
}

void GlicMetrics::OnUserInputSubmitted(mojom::WebClientMode mode) {
  if (!fre_accepted_time_.is_null()) {
    base::TimeDelta delta = base::TimeTicks::Now() - fre_accepted_time_;
    base::RecordAction(base::UserMetricsAction("Glic.Fre.InputSubmitted"));
    base::UmaHistogramLongTimes("Glic.FreToFirstQueryTime", delta);
    base::UmaHistogramCustomTimes("Glic.FreToFirstQueryTimeMax24H", delta,
                                  base::Milliseconds(1), base::Hours(24), 50);
    base::UmaHistogramEnumeration("Glic.Fre.UserInput.InvocationSource",
                                  onboarding_invocation_source_);
    fre_accepted_time_ = base::TimeTicks();
  }

  if (base::FeatureList::IsEnabled(
          features::kGlicFixTimeToFirstQueryKillSwitch)) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Glic.Session.InputSubmit.BrowserActiveState",
      browser_activity_observer_->GetBrowserActiveState());
  base::RecordAction(base::UserMetricsAction("GlicResponseInputSubmit"));
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

void GlicMetrics::OnGlicWindowStartedOpening(bool attached,
                                             mojom::InvocationSource source) {
  // With the exception of setting invocation_source_ and OnInstanceOpened,
  // everything in this method is deprecated post multi-instance side panel.
  // It's kept for now to reduce merge conflicts.

  base::RecordAction(base::UserMetricsAction("GlicSessionBegin"));
  show_start_time_ = base::TimeTicks::Now();
  session_start_time_ = base::TimeTicks::Now();
  invocation_source_ = source;
  base::UmaHistogramBoolean("Glic.Session.Open.Attached", attached);
  base::UmaHistogramEnumeration("Glic.Session.Open.InvocationSource", source);

  // This method depends on first setting invocation_source_. This is used for
  // trust-first FRE metrics.
  OnInstanceOpened();

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
#if !BUILDFLAG(IS_ANDROID)
  base::UmaHistogramEnumeration(
      "Glic.PositionOnChrome.OnOpen",
      GetChromeRelativePositionOfPoint(browser, glic_bounds.CenterPoint()));
#endif
}

void GlicMetrics::OnGlicWindowResize() {
  base::RecordAction(base::UserMetricsAction("GlicPanelResized"));
}

void GlicMetrics::OnWidgetUserResizeStarted() {
  base::RecordAction(base::UserMetricsAction("GlicPanelUserResizeStarted"));
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
#if !BUILDFLAG(IS_ANDROID)
  base::UmaHistogramEnumeration(
      "Glic.PositionOnChrome.OnClose",
      GetChromeRelativePositionOfPoint(last_active_browser,
                                       glic_bounds.CenterPoint()));
#endif
  metrics::ProfileMetricsService* profile_metrics_service =
      ProfileMetricsServiceFactory::GetForProfile(profile_);
  CHECK(profile_metrics_service);
  profile_metrics_service->UmaHistogramCounts1000("Glic.Session.ResponseCount",
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
  bool has_audio = inputs_modes_used_.contains(mojom::WebClientMode::kAudio);
  bool has_text = inputs_modes_used_.contains(mojom::WebClientMode::kText);
  if (has_audio && has_text) {
    modes_used = InputModesUsed::kTextAndAudio;
  } else if (has_audio) {
    modes_used = InputModesUsed::kOnlyAudio;
  } else if (has_text) {
    modes_used = InputModesUsed::kOnlyText;
  }
  inputs_modes_used_.clear();
  base::UmaHistogramEnumeration("Glic.Session.InputModesUsed", modes_used);

  OnInstanceClosed();

  glic_window_size_timer_.Stop();
  profile_->GetPrefs()->SetTime(prefs::kGlicWindowLastDismissedTime,
                                base::Time::Now());
}

void GlicMetrics::LogClosedCaptionsShown() {
  bool pref_enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicClosedCaptioningEnabled);
  base::UmaHistogramBoolean("Glic.Response.ClosedCaptionsShown", pref_enabled);
}

void GlicMetrics::OnShareImageStarted() {
  share_image_start_time_ = base::TimeTicks::Now();
}

void GlicMetrics::OnShareImageComplete(ShareImageResult result) {
  if (!share_image_start_time_.is_null() &&
      result == ShareImageResult::kSentImageToClient) {
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
  const actor::ActorTask* task =
      actor::ActorKeyedService::Get(profile_)->GetTaskFromTab(*tab);
  // Record user action if the tab is associated with an ActorTask.
  if (task) {
    base::RecordAction(
        base::UserMetricsAction("Glic.Instance.TaskTabForegrounded"));
  }
}

void GlicMetrics::SetControllersWithInstance(
    GlicInstance* glic_instance,
    GlicSharingManagerInternal* sharing_manager) {
  delegate_ = std::make_unique<DelegateMultiInstanceImpl>(
      glic_instance, sharing_manager, profile_->GetPrefs());
}
void GlicMetrics::ClearControllers() {
  delegate_ = std::make_unique<DummyDelegateImpl>();
}
void GlicMetrics::SetDelegateForTesting(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void GlicMetrics::DidRequestContextFromTab(tabs::TabInterface& tab) {
  if (content::WebContents* contents = tab.GetContents()) {
    last_tab_context_source_id_ =
        contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  }
}

void GlicMetrics::SetWebClientMode(mojom::WebClientMode mode) {
  input_mode_ = mode;
}

void GlicMetrics::OnImpressionTimerFired() {
  if (!enabling_->IsAllowed()) {
    EntryPointStatus impression;
    if (CheckFreStatus(enabling_, prefs::FreStatus::kNotStarted)) {
      // Profile not eligible, and not started FRE
      impression = EntryPointStatus::kBeforeFreNotEligible;
    } else if (CheckFreStatus(enabling_, prefs::FreStatus::kIncomplete)) {
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
  if (CheckFreStatus(enabling_, prefs::FreStatus::kNotStarted)) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status",
                                  EntryPointStatus::kBeforeFreAndEligible);
    return;
  }

  // Profile eligible, started but not completed FRE
  if (CheckFreStatus(enabling_, prefs::FreStatus::kIncomplete)) {
    base::UmaHistogramEnumeration("Glic.EntryPoint.Status",
                                  EntryPointStatus::kIncompleteFreAndEligible);
    return;
  }

  // Profile eligible and completed FRE
  EntryPointStatus impression;
  auto enablement = GlicEnabling::EnablementForProfile(profile_);
  if (enablement.anchor_entrypoint_override_active) {
    impression = EntryPointStatus::kAfterFreAnchoredButIneligible;
  } else {
#if BUILDFLAG(IS_ANDROID)
    bool is_bottom_bar_enabled = false;
    bool is_mtb_enabled = false;
    GlicKeyedService* service = GlicKeyedService::Get(profile_);
    if (service) {
      is_bottom_bar_enabled = service->IsBottomBarEnabled();
      if (!is_bottom_bar_enabled) {
        is_mtb_enabled = service->IsGlicShortcutActive();
      }
    }
    if (is_mtb_enabled || is_bottom_bar_enabled) {
      impression = EntryPointStatus::kAfterFreBrowserOnly;
    } else {
      impression = EntryPointStatus::kAfterFreThreeDotOnly;
    }
#else
    bool is_pinned =
        profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
    bool is_os_entrypoint_enabled =
        g_browser_process->local_state()->GetBoolean(
            prefs::kGlicLauncherEnabled);
    if (is_pinned && is_os_entrypoint_enabled) {
      impression = EntryPointStatus::kAfterFreBrowserAndOs;
    } else if (is_pinned) {
      impression = EntryPointStatus::kAfterFreBrowserOnly;
    } else if (is_os_entrypoint_enabled) {
      impression = EntryPointStatus::kAfterFreOsOnly;
    } else {
      impression = EntryPointStatus::kAfterFreThreeDotOnly;
    }
#endif
  }
  // TODO(crbug.com/520136927): Move this metric to glic_metrics_provider.cc
  // when glic_metrics.cc is deleted.
  base::UmaHistogramEnumeration("Glic.EntryPoint.Status", impression);

#if !BUILDFLAG(IS_ANDROID)
  ui::Accelerator saved_hotkey =
      glic::GlicLauncherConfiguration::GetGlobalHotkey();
  base::UmaHistogramBoolean("Glic.OsEntrypoint.Settings.ShortcutStatus",
                            saved_hotkey != ui::Accelerator());
#endif
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
  if (!recorded_startup_enablement_ && enabling_->IsAllowed()) {
    RecordStartupEnablement();
  }

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

void GlicMetrics::OnTabPinnedForSharing(GlicTabPinnedForSharingResult result) {
  base::UmaHistogramEnumeration("Glic.Sharing.TabPinnedForSharing", result);
}

void GlicMetrics::RecordStartupEnablement() {
  base::TimeTicks startup_time =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  if (startup_time.is_null()) {
    return;
  }

  base::TimeDelta delta = base::TimeTicks::Now() - startup_time;
  base::UmaHistogramLongTimes("Glic.ProfileEnablement.TimeToEnabledFromStartup",
                              delta);
  recorded_startup_enablement_ = true;
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

#if !BUILDFLAG(IS_ANDROID)
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

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace glic
