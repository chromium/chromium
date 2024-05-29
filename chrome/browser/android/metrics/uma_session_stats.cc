// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/uma_session_stats.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/android/metrics/android_session_durations_service.h"
#include "chrome/browser/android/metrics/android_session_durations_service_factory.h"
#include "chrome/browser/android/preferences/shared_preferences_migrator_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/ukm_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/UmaSessionStats_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::UserMetricsAction;

namespace {
// Used to keep the state of whether we should consider metric consent enabled.
// This is used/read only within the ChromeMetricsServiceAccessor methods.
bool g_metrics_consent_for_testing = false;
}  // namespace

namespace {
// Counter for the number of times onPreCreate and onResume were called between
// foreground sessions that reach native code. The code PXRY means:
// * onPreCreate was called X times
// * onResume was called Y times
// * the counters are capped at 3, so that value means "3 or more".
enum class ChromeActivityCounter : int32_t {
  P0R0 = 0,
  P0R1 = 1,
  P0R2 = 2,
  P0R3 = 3,
  P1R0 = 4,
  P1R1 = 5,
  P1R2 = 6,
  P1R3 = 7,
  P2R0 = 8,
  P2R1 = 9,
  P2R2 = 10,
  P2R3 = 11,
  P3R0 = 12,
  P3R1 = 13,
  P3R2 = 14,
  P3R3 = 15,
  kMaxValue = 15,
};
}  // namespace

void UmaSessionStats::UmaResumeSession(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  DCHECK(g_browser_process);
  if (++active_session_count_ == 1) {
    const bool had_background_session =
        session_time_tracker_.BeginForegroundSession();

    // Tell the metrics services that the application resumes.
    metrics::MetricsService* metrics = g_browser_process->metrics_service();
    if (metrics) {
      // Forcing a new log allows foreground and background metrics can be
      // separated in analysis.
      const bool force_new_log = base::FeatureList::IsEnabled(
                                     chrome::android::kUmaBackgroundSessions) &&
                                 had_background_session;

      metrics->OnAppEnterForeground(force_new_log);
    }
    // Report background session time if it wasn't already reported by
    // OnAppEnterForeground() -> ProvideCurrentSessionData().
    session_time_tracker_.ReportBackgroundSessionTime();

    ukm::UkmService* ukm_service =
        g_browser_process->GetMetricsServicesManager()->GetUkmService();
    if (ukm_service)
      ukm_service->OnAppEnterForeground();

    AndroidSessionDurationsServiceFactory::OnAppEnterForeground(
        session_time_tracker_.session_start_time());
  }
}

void UmaSessionStats::UmaEndSession(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  --active_session_count_;
  DCHECK_GE(active_session_count_, 0);

  if (active_session_count_ == 0) {
    const base::TimeDelta duration =
        session_time_tracker_.EndForegroundSession();

    DCHECK(g_browser_process);
    // Tell the metrics services they were cleanly shutdown.
    metrics::MetricsService* metrics = g_browser_process->metrics_service();
    if (metrics) {
      const bool keep_reporting =
          base::FeatureList::IsEnabled(chrome::android::kUmaBackgroundSessions);
      metrics->OnAppEnterBackground(keep_reporting);
    }
    ukm::UkmService* ukm_service =
        g_browser_process->GetMetricsServicesManager()->GetUkmService();
    if (ukm_service)
      ukm_service->OnAppEnterBackground();

    AndroidSessionDurationsServiceFactory::OnAppEnterBackground(duration);

    // Note: Keep the line below after |metrics->OnAppEnterBackground()|.
    // Otherwise, |ProvideCurrentSessionData()| may report a small timeslice of
    // background session time toward the previous log.
    session_time_tracker_.BeginBackgroundSession();
  }
}

void UmaSessionStats::ProvideCurrentSessionData() {
  base::UmaHistogramBoolean("Session.IsActive", active_session_count_ != 0);

  // We record Session.Background.TotalDuration here to ensure each UMA log
  // containing a background session contains this histogram.
  session_time_tracker_.AccumulateBackgroundSessionTime();
  session_time_tracker_.ReportBackgroundSessionTime();
}

// static
UmaSessionStats* UmaSessionStats::GetInstance() {
  static base::NoDestructor<UmaSessionStats> instance;
  return instance.get();
}

// static
bool UmaSessionStats::HasVisibleActivity() {
  return Java_UmaSessionStats_hasVisibleActivity(
      base::android::AttachCurrentThread());
}

// Called on startup. If there is an activity, do nothing because a foreground
// session will be created naturally. Otherwise, begin recording a background
// session.
// static
void UmaSessionStats::OnStartup() {
  if (!UmaSessionStats::HasVisibleActivity()) {
    GetInstance()->session_time_tracker_.BeginBackgroundSession();
  }
}

// static
void UmaSessionStats::RegisterSyntheticFieldTrial(
    const std::string& trial_name,
    const std::string& group_name,
    variations::SyntheticTrialAnnotationMode annotation_mode) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name, annotation_mode);
}

// static
bool UmaSessionStats::IsBackgroundSessionStartForTesting() {
  return !GetInstance()
              ->session_time_tracker_.background_session_start_time()
              .is_null();
}

void UmaSessionStats::EmitAndResetCounters() {
  std::optional<int> on_postcreate_counter =
      android::shared_preferences::GetAndClearInt(
          "Chrome.UMA.OnPostCreateCounter2");
  std::optional<int> on_resume_counter =
      android::shared_preferences::GetAndClearInt(
          "Chrome.UMA.OnResumeCounter2");
  int on_create_count = std::min(on_postcreate_counter.value_or(0), 3);
  int on_resume_count = std::min(on_resume_counter.value_or(0), 3);
  ChromeActivityCounter count_code =
      static_cast<ChromeActivityCounter>(4 * on_create_count + on_resume_count);
  UMA_HISTOGRAM_ENUMERATION("UMA.AndroidPreNative.ChromeActivityCounter2",
                            count_code);
}

void UmaSessionStats::SessionTimeTracker::AccumulateBackgroundSessionTime() {
  // No time spent in background since the last call to
  // |AccumulateBackgroundSessionTime()|.
  if (background_session_start_time_.is_null())
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta duration = now - background_session_start_time_;
  background_session_accumulated_time_ += duration;

  background_session_start_time_ = now;
}

void UmaSessionStats::SessionTimeTracker::ReportBackgroundSessionTime() {
  if (background_session_accumulated_time_.is_zero())
    return;

  // This histogram is used in analysis to determine if an uploaded log
  // represents background activity. For this reason, this histogram may be
  // recorded more than once per 'background session'.
  UMA_HISTOGRAM_CUSTOM_TIMES("Session.Background.TotalDuration",
                             background_session_accumulated_time_,
                             base::Milliseconds(1), base::Hours(24), 50);
  background_session_accumulated_time_ = base::TimeDelta();
}

bool UmaSessionStats::SessionTimeTracker::BeginForegroundSession() {
  // Emit onPostCreate & onResume counters. This is done early in the session
  // to ensure that these are captured even if the session is not ended
  // cleanly.
  UmaSessionStats::EmitAndResetCounters();
  AccumulateBackgroundSessionTime();
  background_session_start_time_ = {};
  session_start_time_ = base::TimeTicks::Now();
  return !background_session_accumulated_time_.is_zero();
}

base::TimeDelta UmaSessionStats::SessionTimeTracker::EndForegroundSession() {
  base::TimeDelta duration = base::TimeTicks::Now() - session_start_time_;

  // Note: This metric is recorded separately on desktop in
  // DesktopSessionDurationTracker::EndSession.
  UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration", duration);
  UMA_HISTOGRAM_CUSTOM_TIMES("Session.TotalDurationMax1Day", duration,
                             base::Milliseconds(1), base::Hours(24), 50);
  return duration;
}

void UmaSessionStats::SessionTimeTracker::BeginBackgroundSession() {
  background_session_start_time_ = base::TimeTicks::Now();
}

// Updates metrics reporting state managed by native code. This should only be
// called when consent is changing, and UpdateMetricsServiceState() should be
// called immediately after for metrics services to be started or stopped as
// needed. This is enforced by UmaSessionStats.changeMetricsReportingConsent on
// the Java side.
static void JNI_UmaSessionStats_ChangeMetricsReportingConsent(
    JNIEnv*,
    jboolean consent,
    jint called_from) {
  UpdateMetricsPrefsOnPermissionChange(
      consent, static_cast<ChangeMetricsReportingStateCalledFrom>(called_from));

  // This function ensures a consent file in the data directory is either
  // created, or deleted, depending on consent. Starting up metrics services
  // will ensure that the consent file contains the ClientID. The ID is passed
  // to the renderer for crash reporting when things go wrong.
  GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(GoogleUpdateSettings::SetCollectStatsConsent),
          consent));
}

// Initialize the local consent bool variable to false. Used only for testing.
static void JNI_UmaSessionStats_InitMetricsAndCrashReportingForTesting(
    JNIEnv*) {
  DCHECK(g_browser_process);

  g_metrics_consent_for_testing = false;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &g_metrics_consent_for_testing);
}

// Clears the boolean consent pointer for ChromeMetricsServiceAccessor to
// original setting. Used only for testing.
static void JNI_UmaSessionStats_UnsetMetricsAndCrashReportingForTesting(
    JNIEnv*) {
  DCHECK(g_browser_process);

  g_metrics_consent_for_testing = false;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

// Updates the metrics consent bit to |consent|. This is separate from
// InitMetricsAndCrashReportingForTesting as the Set isn't meant to be used
// repeatedly. Used only for testing.
static void JNI_UmaSessionStats_UpdateMetricsAndCrashReportingForTesting(
    JNIEnv*,
    jboolean consent) {
  DCHECK(g_browser_process);

  g_metrics_consent_for_testing = consent;
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
}

// Starts/stops the MetricsService based on existing consent and upload
// preferences.
// There are three possible states:
// * Logs are being recorded and being uploaded to the server.
// * Logs are being recorded, but not being uploaded to the server.
//   This happens when we've got permission to upload on Wi-Fi but we're on a
//   mobile connection (for example).
// * Logs are neither being recorded or uploaded.
// If logs aren't being recorded, then |may_upload| is ignored.
//
// This can be called at any time when consent hasn't changed, such as
// connection type change, or start up. If consent has changed, then
// ChangeMetricsReportingConsent() should be called first.
static void JNI_UmaSessionStats_UpdateMetricsServiceState(
    JNIEnv*,
    jboolean may_upload) {
  // This will also apply the consent state, taken from Chrome Local State
  // prefs.
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
      may_upload);
}

static void JNI_UmaSessionStats_RegisterExternalExperiment(
    JNIEnv* env,
    const JavaParamRef<jintArray>& jexperiment_ids,
    jboolean override_existing_ids) {
  std::vector<int> experiment_ids;
  // A null |jexperiment_ids| is the same as an empty list.
  if (jexperiment_ids) {
    base::android::JavaIntArrayToIntVector(env, jexperiment_ids,
                                           &experiment_ids);
  }

  auto override_mode =
      override_existing_ids
          ? variations::SyntheticTrialRegistry::kOverrideExistingIds
          : variations::SyntheticTrialRegistry::kDoNotOverrideExistingIds;

  g_browser_process->metrics_service()
      ->GetSyntheticTrialRegistry()
      ->RegisterExternalExperiments(experiment_ids, override_mode);
}

static void JNI_UmaSessionStats_RegisterSyntheticFieldTrial(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtrial_name,
    const JavaParamRef<jstring>& jgroup_name,
    int annotation_mode) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  std::string group_name(ConvertJavaStringToUTF8(env, jgroup_name));
  UmaSessionStats::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      static_cast<variations::SyntheticTrialAnnotationMode>(annotation_mode));
}

static void JNI_UmaSessionStats_RecordTabCountPerLoad(
    JNIEnv*,
    jint num_tabs) {
  // Record how many tabs total are open.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", num_tabs, 1, 200, 50);
}

static void JNI_UmaSessionStats_RecordPageLoaded(
    JNIEnv*,
    jboolean is_desktop_user_agent) {
  // Should be called whenever a page has been loaded.
  base::RecordAction(UserMetricsAction("MobilePageLoaded"));
  if (is_desktop_user_agent) {
    base::RecordAction(UserMetricsAction("MobilePageLoadedDesktopUserAgent"));
  }
}

static void JNI_UmaSessionStats_RecordPageLoadedWithAccessory(JNIEnv*) {
  base::RecordAction(UserMetricsAction("MobilePageLoadedWithAccessory"));
}

static void JNI_UmaSessionStats_RecordPageLoadedWithKeyboard(JNIEnv*) {
  base::RecordAction(UserMetricsAction("MobilePageLoadedWithKeyboard"));
}

static void JNI_UmaSessionStats_RecordPageLoadedWithMouse(JNIEnv*) {
  base::RecordAction(UserMetricsAction("MobilePageLoadedWithMouse"));
}

static void JNI_UmaSessionStats_RecordPageLoadedWithToEdge(JNIEnv*) {
  base::RecordAction(UserMetricsAction("MobilePageLoadedWithToEdge"));
}

static jlong JNI_UmaSessionStats_Init(JNIEnv* env) {
  // We should have only one UmaSessionStats instance.
  return reinterpret_cast<intptr_t>(UmaSessionStats::GetInstance());
}
