// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_annotation_monitor.h"

#include <utility>

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/regmon/regmon_client.h"
#include "chromeos/dbus/regmon/regmon_service.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

NetworkAnnotationMonitor::NetworkAnnotationMonitor() = default;
NetworkAnnotationMonitor::~NetworkAnnotationMonitor() = default;

void NetworkAnnotationMonitor::Report(int32_t hash_code) {
  // Multi-profile is not currently supported, so only run on ChromeOS for now.
  static_assert(BUILDFLAG(IS_CHROMEOS));

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // Get blocklist prefs from the current active profile, which on ChromeOS
  // should be the only profile based on the above check.
  const auto* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  if (!active_session) {
    return;
  }

  // The user must exist always.
  const auto& user = CHECK_DEREF(
      user_manager::UserManager::Get()->FindUser(active_session->account_id()));
  if (!user.is_profile_created()) {
    // This happens during the user log-in. User session is created at earlier
    // timing, but its corresponding Profile is created asynchronously later.
    return;
  }

  // Ignore any network calls not in the blocklist.
  const base::DictValue& blocklist =
      user.GetProfilePrefs()->GetDict(prefs::kNetworkAnnotationBlocklist);
  if (!blocklist.contains(base::NumberToString(hash_code))) {
    return;
  }

  chromeos::RegmonClient* client = chromeos::RegmonClient::Get();
  if (!client) {
    return;
  }

  regmon::PolicyViolation policy_violation;
  policy_violation.set_policy(::regmon::PolicyViolation::POLICY_UNSPECIFIED);
  policy_violation.set_annotation_hash(hash_code);

  regmon::RecordPolicyViolationRequest request =
      regmon::RecordPolicyViolationRequest();
  *request.mutable_violation() = policy_violation;
  client->RecordPolicyViolation(request);

  // Publish time metric for this function.
  UMA_HISTOGRAM_TIMES("ChromeOS.Regmon.ReportViolationTime",
                      base::TimeTicks::Now() - start_time);
}

mojo::PendingRemote<network::mojom::NetworkAnnotationMonitor>
NetworkAnnotationMonitor::GetClient() {
  // Reset receiver if already bound. This can happen if the Network Service
  // crashed and has been restarted.
  if (receiver_.is_bound()) {
    receiver_.reset();
  }

  mojo::PendingRemote<network::mojom::NetworkAnnotationMonitor> client;
  receiver_.Bind(client.InitWithNewPipeAndPassReceiver());
  return client;
}

void NetworkAnnotationMonitor::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
}
