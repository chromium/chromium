// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "components/session_manager/core/session_manager.h"

namespace {

constexpr char kFamilyLinkUserLogSegmentHistogramName[] =
    "FamilyLinkUser.LogSegment";

}  // namespace

FamilyLinkUserMetricsProvider::FamilyLinkUserMetricsProvider() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // The |session_manager| is nullptr only for unit tests.
  if (session_manager)
    session_manager->AddObserver(this);
}

FamilyLinkUserMetricsProvider::~FamilyLinkUserMetricsProvider() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // The |session_manager| is nullptr only for unit tests.
  if (session_manager)
    session_manager->RemoveObserver(this);
}

void FamilyLinkUserMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  // This function is called at unpredictable intervals throughout the Chrome
  // session, so guarantee it will never crash.
  if (!log_segment_)
    return;
  base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                log_segment_.value());
}

void FamilyLinkUserMetricsProvider::OnUserSessionStarted(bool is_primary_user) {
  // TODO(crbug.com/1251622): Implement user session segmentation based on
  // Capabilities API.
}

void FamilyLinkUserMetricsProvider::SetLogSegment(LogSegment log_segment) {
  log_segment_ = log_segment;
}
