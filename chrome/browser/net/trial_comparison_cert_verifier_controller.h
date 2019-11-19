// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_TRIAL_COMPARISON_CERT_VERIFIER_CONTROLLER_H_
#define CHROME_BROWSER_NET_TRIAL_COMPARISON_CERT_VERIFIER_CONTROLLER_H_

#include <stdint.h>

#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"
#include "services/network/public/mojom/trial_comparison_cert_verifier.mojom.h"

class Profile;

class TrialComparisonCertVerifierController
    : public network::mojom::TrialComparisonCertVerifierReportClient {
 public:
  // Creates a TrialComparisonCertVerifierController using |profile| for
  // preferences and reporting.
  // |profile| must outlive the TrialComparisonCertVerifierController.
  explicit TrialComparisonCertVerifierController(Profile* profile);
  ~TrialComparisonCertVerifierController() override;

  // Returns true if the trial could potentially be enabled for |profile|;
  static bool MaybeAllowedForProfile(Profile* profile);

  // Adds a client to the controller, sending trial configuration updates to
  // |config_client|, and receiving trial reports from |report_client_receiver|.
  void AddClient(mojo::PendingRemote<
                     network::mojom::TrialComparisonCertVerifierConfigClient>
                     config_client,
                 mojo::PendingReceiver<
                     network::mojom::TrialComparisonCertVerifierReportClient>
                     report_client_receiver);

  // Returns true if the trial is enabled and SBER flag is set for this
  // profile.
  bool IsAllowed() const;

  // TrialComparisonCertVerifierReportClient implementation:
  void SendTrialReport(
      const std::string& hostname,
      const scoped_refptr<net::X509Certificate>& unverified_cert,
      bool enable_rev_checking,
      bool require_rev_checking_local_anchors,
      bool enable_sha1_local_anchors,
      bool disable_symantec_enforcement,
      const net::CertVerifyResult& primary_result,
      const net::CertVerifyResult& trial_result,
      network::mojom::CertVerifierDebugInfoPtr debug_info) override;

  static void SetFakeOfficialBuildForTesting(bool fake_official_build);

 private:
  void RefreshState();

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;

  mojo::ReceiverSet<network::mojom::TrialComparisonCertVerifierReportClient>
      receiver_set_;

  mojo::RemoteSet<network::mojom::TrialComparisonCertVerifierConfigClient>
      config_client_set_;

  DISALLOW_COPY_AND_ASSIGN(TrialComparisonCertVerifierController);
};

#endif  // CHROME_BROWSER_NET_TRIAL_COMPARISON_CERT_VERIFIER_CONTROLLER_H_
