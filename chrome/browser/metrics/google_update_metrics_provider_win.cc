// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/google_update_metrics_provider_win.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_details.h"
#include "third_party/metrics_proto/system_profile.pb.h"

typedef metrics::SystemProfileProto::GoogleUpdate::ProductInfo ProductInfo;

namespace {

// Helper function for checking if this is an official build. Used instead of
// the macro to allow checking for successful code compilation on non-official
// builds.
bool IsGoogleChromeBuild() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void ProductDataToProto(const GoogleUpdateSettings::ProductData& product_data,
                        ProductInfo* product_info) {
  product_info->set_version(product_data.version);
  product_info->set_last_update_success_timestamp(
      product_data.last_success.ToTimeT());
  product_info->set_last_error(product_data.last_error_code);
  product_info->set_last_extra_error(product_data.last_extra_code);
  if (ProductInfo::InstallResult_IsValid(product_data.last_result)) {
    product_info->set_last_result(
        static_cast<ProductInfo::InstallResult>(product_data.last_result));
  }
}

}  // namespace

GoogleUpdateMetricsProviderWin::GoogleUpdateMetricsProviderWin() {}

GoogleUpdateMetricsProviderWin::~GoogleUpdateMetricsProviderWin() {
}

void GoogleUpdateMetricsProviderWin::AsyncInit(
    const base::Closure& done_callback) {
  if (!IsGoogleChromeBuild()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, done_callback);
    return;
  }

  // Schedules a task on a blocking pool thread to gather Google Update
  // statistics (requires Registry reads).
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&GoogleUpdateMetricsProviderWin::GetGoogleUpdateDataBlocking),
      base::Bind(&GoogleUpdateMetricsProviderWin::ReceiveGoogleUpdateData,
                 weak_ptr_factory_.GetWeakPtr(), done_callback));
}

void GoogleUpdateMetricsProviderWin::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  // Do nothing for chromium builds.
  if (!IsGoogleChromeBuild())
    return;
  // Convert wstring to string.
  std::string update_cohort_name = base::WideToUTF8(
      install_static::InstallDetails::Get().update_cohort_name());
  // TODO(nikunjb): Once update_cohort_name is added to system profile
  // update the code here.
  base::UmaHistogramSparse("GoogleUpdate.InstallDetails.UpdateCohort",
                           base::HashMetricName(update_cohort_name));
  metrics::SystemProfileProto::GoogleUpdate* google_update =
      system_profile_proto->mutable_google_update();

  google_update->set_is_system_install(
      google_update_metrics_.is_system_install);

  if (!google_update_metrics_.last_started_automatic_update_check.is_null()) {
    google_update->set_last_automatic_start_timestamp(
        google_update_metrics_.last_started_automatic_update_check.ToTimeT());
  }

  if (!google_update_metrics_.last_checked.is_null()) {
    google_update->set_last_update_check_timestamp(
        google_update_metrics_.last_checked.ToTimeT());
  }

  if (!google_update_metrics_.google_update_data.version.empty()) {
    ProductDataToProto(google_update_metrics_.google_update_data,
                       google_update->mutable_google_update_status());
  }

  if (!google_update_metrics_.product_data.version.empty()) {
    ProductDataToProto(google_update_metrics_.product_data,
                       google_update->mutable_client_status());
  }
}

GoogleUpdateMetricsProviderWin::GoogleUpdateMetrics::GoogleUpdateMetrics()
    : is_system_install(false) {
}

GoogleUpdateMetricsProviderWin::GoogleUpdateMetrics::~GoogleUpdateMetrics() {
}

// static
GoogleUpdateMetricsProviderWin::GoogleUpdateMetrics
GoogleUpdateMetricsProviderWin::GetGoogleUpdateDataBlocking() {
  GoogleUpdateMetrics google_update_metrics;

  if (!IsGoogleChromeBuild())
    return google_update_metrics;

  const bool is_system_install = GoogleUpdateSettings::IsSystemInstall();
  google_update_metrics.is_system_install = is_system_install;
  google_update_metrics.last_started_automatic_update_check =
      GoogleUpdateSettings::GetGoogleUpdateLastStartedAU(is_system_install);
  google_update_metrics.last_checked =
      GoogleUpdateSettings::GetGoogleUpdateLastChecked(is_system_install);
  GoogleUpdateSettings::GetUpdateDetailForGoogleUpdate(
      &google_update_metrics.google_update_data);
  GoogleUpdateSettings::GetUpdateDetail(&google_update_metrics.product_data);
  return google_update_metrics;
}

void GoogleUpdateMetricsProviderWin::ReceiveGoogleUpdateData(
    const base::Closure& done_callback,
    const GoogleUpdateMetrics& google_update_metrics) {
  google_update_metrics_ = google_update_metrics;
  done_callback.Run();
}
