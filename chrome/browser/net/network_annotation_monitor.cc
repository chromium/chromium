// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_annotation_monitor.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/regmon/regmon_client.h"
#include "chromeos/dbus/regmon/regmon_service.pb.h"
#include "components/prefs/pref_service.h"

NetworkAnnotationMonitor::NetworkAnnotationMonitor() = default;
NetworkAnnotationMonitor::~NetworkAnnotationMonitor() = default;

void NetworkAnnotationMonitor::Report(int32_t hash_code) {
  // Multi-profile is not currently supported, so only run on ChromeOS for now.
  static_assert(BUILDFLAG(IS_CHROMEOS));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros allows multi-profile if enabled by policy, so skip reporting in this
  // case. In the future we could consider using ProfileNetworkContext for this.
  if (profiles::AreSecondaryProfilesAllowed()) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // Get blocklist prefs from the current active profile, which on ChromeOS
  // should be the only profile based on the above check.
  const base::Value::Dict& blocklist =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetDict(
          prefs::kNetworkAnnotationBlocklist);

  // Ignore any network calls not in the blocklist.
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
