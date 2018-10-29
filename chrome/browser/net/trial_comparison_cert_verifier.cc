// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/trial_comparison_cert_verifier.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/ssl/certificate_error_report.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"

// Certificate reports are only sent from official builds, but this flag can be
// set by tests.
static bool g_is_fake_official_build_for_cert_verifier_testing = false;

namespace {

bool CheckTrialEligibility(void* profile_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // g_browser_process is valid until after all threads are stopped. So it must
  // be valid if the CheckTrialEligibility task got to run.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_id))
    return false;
  const Profile* profile = reinterpret_cast<const Profile*>(profile_id);
  const PrefService& prefs = *profile->GetPrefs();

  // Only allow on non-incognito profiles which have SBER opt-in set.
  // See design doc for more details:
  // https://docs.google.com/document/d/1AM1CD42bC6LHWjKg-Hkid_RLr2DH6OMzstH9-pGSi-g
  return !profile->IsOffTheRecord() &&
         safe_browsing::IsExtendedReportingEnabled(prefs);
}

void SendTrialVerificationReport(void* profile_id,
                                 const net::CertVerifier::Config& config,
                                 const net::CertVerifier::RequestParams& params,
                                 const net::CertVerifyResult& primary_result,
                                 const net::CertVerifyResult& trial_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile_id))
    return;
  Profile* profile = reinterpret_cast<Profile*>(profile_id);

  CertificateErrorReport report(params.hostname(), *params.certificate(),
                                config, primary_result, trial_result);

  report.AddNetworkTimeInfo(g_browser_process->network_time_tracker());
  report.AddChromeChannel(chrome::GetChannel());

  std::string serialized_report;
  if (!report.Serialize(&serialized_report))
    return;

  CertificateReportingServiceFactory::GetForBrowserContext(profile)->Send(
      serialized_report);
}

std::unique_ptr<base::Value> TrialVerificationJobResultCallback(
    bool trial_success,
    net::NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  results->SetKey("trial_success", base::Value(trial_success));
  return std::move(results);
}

bool CertVerifyResultEqual(const net::CertVerifyResult& a,
                           const net::CertVerifyResult& b) {
  return std::tie(a.cert_status, a.is_issued_by_known_root) ==
             std::tie(b.cert_status, b.is_issued_by_known_root) &&
         (!!a.verified_cert == !!b.verified_cert) &&
         (!a.verified_cert ||
          a.verified_cert->EqualsIncludingChain(b.verified_cert.get()));
}

scoped_refptr<net::ParsedCertificate> ParsedCertificateFromBuffer(
    CRYPTO_BUFFER* cert_handle,
    net::CertErrors* errors) {
  return net::ParsedCertificate::Create(
      bssl::UpRef(cert_handle),
      net::x509_util::DefaultParseCertificateOptions(), errors);
}

net::ParsedCertificateList ParsedCertificateListFromX509Certificate(
    const net::X509Certificate* cert) {
  net::CertErrors parsing_errors;

  net::ParsedCertificateList certs;
  scoped_refptr<net::ParsedCertificate> target =
      ParsedCertificateFromBuffer(cert->cert_buffer(), &parsing_errors);
  if (!target)
    return {};
  certs.push_back(target);

  for (const auto& buf : cert->intermediate_buffers()) {
    scoped_refptr<net::ParsedCertificate> intermediate =
        ParsedCertificateFromBuffer(buf.get(), &parsing_errors);
    if (!intermediate)
      return {};
    certs.push_back(intermediate);
  }

  return certs;
}

// Tests whether cert has multiple EV policies, and at least one matches the
// root. This is not a complete test of EV, but just enough to give a possible
// explanation as to why the platform verifier did not validate as EV while
// builtin did. (Since only the builtin verifier correctly handles multiple
// candidate EV policies.)
bool CertHasMultipleEVPoliciesAndOneMatchesRoot(
    const net::X509Certificate* cert) {
  if (cert->intermediate_buffers().empty())
    return false;

  net::ParsedCertificateList certs =
      ParsedCertificateListFromX509Certificate(cert);
  if (certs.empty())
    return false;

  net::ParsedCertificate* leaf = certs.front().get();
  net::ParsedCertificate* root = certs.back().get();

  if (!leaf->has_policy_oids())
    return false;

  const net::EVRootCAMetadata* ev_metadata =
      net::EVRootCAMetadata::GetInstance();
  std::set<net::der::Input> candidate_oids;
  for (const net::der::Input& oid : leaf->policy_oids()) {
    if (ev_metadata->IsEVPolicyOIDGivenBytes(oid))
      candidate_oids.insert(oid);
  }

  if (candidate_oids.size() <= 1)
    return false;

  net::SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(root->der_cert().AsStringPiece(),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  for (const net::der::Input& oid : candidate_oids) {
    if (ev_metadata->HasEVPolicyOIDGivenBytes(root_fingerprint, oid))
      return true;
  }

  return false;
}

}  // namespace

class TrialComparisonCertVerifier::TrialVerificationJob {
 public:
  TrialVerificationJob(const net::CertVerifier::Config& config,
                       const net::CertVerifier::RequestParams& params,
                       const net::NetLogWithSource& source_net_log,
                       TrialComparisonCertVerifier* cert_verifier,
                       int primary_error,
                       const net::CertVerifyResult& primary_result,
                       void* profile_id)
      : config_(config),
        config_changed_(false),
        params_(params),
        net_log_(net::NetLogWithSource::Make(
            source_net_log.net_log(),
            net::NetLogSourceType::TRIAL_CERT_VERIFIER_JOB)),
        profile_id_(profile_id),
        cert_verifier_(cert_verifier),
        primary_error_(primary_error),
        primary_result_(primary_result) {
    net_log_.BeginEvent(net::NetLogEventType::TRIAL_CERT_VERIFIER_JOB);
    source_net_log.AddEvent(
        net::NetLogEventType::TRIAL_CERT_VERIFIER_JOB_COMPARISON_STARTED,
        net_log_.source().ToEventParametersCallback());
  }

  ~TrialVerificationJob() {
    if (cert_verifier_) {
      net_log_.AddEvent(net::NetLogEventType::CANCELLED);
      net_log_.EndEvent(net::NetLogEventType::TRIAL_CERT_VERIFIER_JOB);
    }
  }

  void Start() {
    // Unretained is safe because trial_request_ will cancel the callback on
    // destruction.
    int rv = cert_verifier_->trial_verifier()->Verify(
        params_, &trial_result_,
        base::BindOnce(&TrialVerificationJob::OnJobCompleted,
                       base::Unretained(this)),
        &trial_request_, net_log_);
    if (rv != net::ERR_IO_PENDING)
      OnJobCompleted(rv);
  }

  void OnConfigChanged() { config_changed_ = true; }

  void Finish(bool is_success, TrialComparisonResult result_code) {
    TrialComparisonCertVerifier* cert_verifier = cert_verifier_;
    cert_verifier_ = nullptr;

    UMA_HISTOGRAM_ENUMERATION("Net.CertVerifier_TrialComparisonResult",
                              result_code);

    net_log_.EndEvent(
        net::NetLogEventType::TRIAL_CERT_VERIFIER_JOB,
        base::BindRepeating(&TrialVerificationJobResultCallback, is_success));

    if (!is_success &&
        !base::GetFieldTrialParamByFeatureAsBool(
            features::kCertDualVerificationTrialFeature, "uma_only", false)) {
      base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
          ->PostTask(FROM_HERE, base::BindOnce(&SendTrialVerificationReport,
                                               profile_id_, config_, params_,
                                               primary_result_, trial_result_));
    }

    // |this| is deleted after RemoveJob returns.
    cert_verifier->RemoveJob(this);
  }

  void FinishSuccess(TrialComparisonResult result_code) {
    Finish(true /* is_success */, result_code);
  }

  void FinishWithError() {
    DCHECK(trial_error_ != primary_error_ ||
           !CertVerifyResultEqual(trial_result_, primary_result_));

    TrialComparisonResult result_code = kInvalid;

    if (primary_error_ == net::OK && trial_error_ == net::OK) {
      result_code = kBothValidDifferentDetails;
    } else if (primary_error_ == net::OK) {
      result_code = kPrimaryValidSecondaryError;
    } else if (trial_error_ == net::OK) {
      result_code = kPrimaryErrorSecondaryValid;
    } else {
      result_code = kBothErrorDifferentDetails;
    }
    Finish(false /* is_success */, result_code);
  }

  void OnJobCompleted(int trial_result_error) {
    DCHECK(primary_result_.verified_cert);
    DCHECK(trial_result_.verified_cert);

    trial_error_ = trial_result_error;

    bool errors_equal = trial_result_error == primary_error_;
    bool details_equal = CertVerifyResultEqual(trial_result_, primary_result_);
    bool trial_success = errors_equal && details_equal;

    if (trial_success) {
      FinishSuccess(kEqual);
      return;
    }

#if defined(OS_MACOSX)
    if (primary_error_ == net::ERR_CERT_REVOKED &&
        !config_.enable_rev_checking &&
        !(primary_result_.cert_status &
          net::CERT_STATUS_REV_CHECKING_ENABLED) &&
        !(trial_result_.cert_status &
          (net::CERT_STATUS_REVOKED | net::CERT_STATUS_REV_CHECKING_ENABLED))) {
      if (config_changed_) {
        FinishSuccess(kIgnoredConfigurationChanged);
        return;
      }
      // CertVerifyProcMac does some revocation checking even if we didn't want
      // it. Try verifying with the trial verifier with revocation checking
      // enabled, see if it then returns REVOKED.

      int rv = cert_verifier_->revocation_trial_verifier()->Verify(
          params_, &reverification_result_,
          base::BindOnce(
              &TrialVerificationJob::OnMacRevcheckingReverificationJobCompleted,
              base::Unretained(this)),
          &reverification_request_, net_log_);
      if (rv != net::ERR_IO_PENDING)
        OnMacRevcheckingReverificationJobCompleted(rv);
      return;
    }
#endif

    const bool chains_equal =
        primary_result_.verified_cert->EqualsIncludingChain(
            trial_result_.verified_cert.get());

    if (!chains_equal &&
        (trial_error_ == net::OK || primary_error_ != net::OK)) {
      if (config_changed_) {
        FinishSuccess(kIgnoredConfigurationChanged);
        return;
      }
      // Chains were different, reverify the trial_result_.verified_cert chain
      // using the platform verifier and compare results again.
      RequestParams reverification_params(trial_result_.verified_cert,
                                          params_.hostname(), params_.flags(),
                                          params_.ocsp_response());

      int rv = cert_verifier_->primary_reverifier()->Verify(
          reverification_params, &reverification_result_,
          base::BindOnce(&TrialVerificationJob::
                             OnPrimaryReverifiyWithSecondaryChainCompleted,
                         base::Unretained(this)),
          &reverification_request_, net_log_);
      if (rv != net::ERR_IO_PENDING)
        OnPrimaryReverifiyWithSecondaryChainCompleted(rv);
      return;
    }

    TrialComparisonResult ignorable_difference =
        IsSynchronouslyIgnorableDifference(primary_error_, primary_result_,
                                           trial_error_, trial_result_);
    if (ignorable_difference != kInvalid) {
      FinishSuccess(ignorable_difference);
      return;
    }

    FinishWithError();
  }

  // Check if the differences between the primary and trial verifiers can be
  // ignored. This only handles differences that can be checked synchronously.
  // If the difference is ignorable, returns the relevant TrialComparisonResult,
  // otherwise returns kInvalid.
  static TrialComparisonResult IsSynchronouslyIgnorableDifference(
      int primary_error,
      const net::CertVerifyResult& primary_result,
      int trial_error,
      const net::CertVerifyResult& trial_result) {
    DCHECK(primary_result.verified_cert);
    DCHECK(trial_result.verified_cert);

    if (primary_error == net::OK &&
        primary_result.verified_cert->intermediate_buffers().empty()) {
      // Platform may support trusting a leaf certificate directly. Builtin
      // verifier does not. See https://crbug.com/814994.
      return kIgnoredLocallyTrustedLeaf;
    }

    const bool chains_equal =
        primary_result.verified_cert->EqualsIncludingChain(
            trial_result.verified_cert.get());

    if (chains_equal && (trial_result.cert_status & net::CERT_STATUS_IS_EV) &&
        !(primary_result.cert_status & net::CERT_STATUS_IS_EV) &&
        (primary_error == trial_error)) {
      // The platform CertVerifyProc impls only check a single potential EV
      // policy from the leaf.  If the leaf had multiple policies, builtin
      // verifier may verify it as EV when the platform verifier did not.
      if (CertHasMultipleEVPoliciesAndOneMatchesRoot(
              trial_result.verified_cert.get())) {
        return kIgnoredMultipleEVPoliciesAndOneMatchesRoot;
      }
    }
    return kInvalid;
  }

#if defined(OS_MACOSX)
  void OnMacRevcheckingReverificationJobCompleted(int reverification_error) {
    if (reverification_error == net::ERR_CERT_REVOKED) {
      FinishSuccess(kIgnoredMacUndesiredRevocationChecking);
      return;
    }
    FinishWithError();
  }
#endif

  void OnPrimaryReverifiyWithSecondaryChainCompleted(int reverification_error) {
    if (reverification_error == trial_error_ &&
        CertVerifyResultEqual(reverification_result_, trial_result_)) {
      // The new result matches the builtin verifier, so this was just
      // a difference in the platform's path-building ability.
      // Ignore the difference.
      FinishSuccess(kIgnoredDifferentPathReVerifiesEquivalent);
      return;
    }

    if (IsSynchronouslyIgnorableDifference(reverification_error,
                                           reverification_result_, trial_error_,
                                           trial_result_) != kInvalid) {
      // The new result matches if ignoring differences. Still use the
      // |kIgnoredDifferentPathReVerifiesEquivalent| code rather than the
      // result of IsSynchronouslyIgnorableDifference, since it's the higher
      // level description of what the difference is in this case.
      FinishSuccess(kIgnoredDifferentPathReVerifiesEquivalent);
      return;
    }

    FinishWithError();
  }

 private:
  const net::CertVerifier::Config config_;
  bool config_changed_;
  const net::CertVerifier::RequestParams params_;
  const net::NetLogWithSource net_log_;
  void* profile_id_;
  TrialComparisonCertVerifier* cert_verifier_;  // Non-owned.

  // Results from the trial verification.
  int trial_error_;
  net::CertVerifyResult trial_result_;
  std::unique_ptr<net::CertVerifier::Request> trial_request_;

  // Saved results of the primary verification.
  int primary_error_;
  const net::CertVerifyResult primary_result_;

  // Results from re-verification attempt.
  net::CertVerifyResult reverification_result_;
  std::unique_ptr<net::CertVerifier::Request> reverification_request_;

  DISALLOW_COPY_AND_ASSIGN(TrialVerificationJob);
};

TrialComparisonCertVerifier::TrialComparisonCertVerifier(
    void* profile_id,
    scoped_refptr<net::CertVerifyProc> primary_verify_proc,
    scoped_refptr<net::CertVerifyProc> trial_verify_proc)
    : profile_id_(profile_id),
      config_id_(0),
      primary_verifier_(
          net::MultiThreadedCertVerifier::CreateForDualVerificationTrial(
              primary_verify_proc,
              // Unretained is safe since the callback won't be called after
              // |primary_verifier_| is destroyed.
              base::BindRepeating(
                  &TrialComparisonCertVerifier::OnPrimaryVerifierComplete,
                  base::Unretained(this)),
              true /* should_record_histograms */)),
      primary_reverifier_(std::make_unique<net::MultiThreadedCertVerifier>(
          primary_verify_proc)),
      trial_verifier_(
          net::MultiThreadedCertVerifier::CreateForDualVerificationTrial(
              trial_verify_proc,
              // Unretained is safe since the callback won't be called after
              // |trial_verifier_| is destroyed.
              base::BindRepeating(
                  &TrialComparisonCertVerifier::OnTrialVerifierComplete,
                  base::Unretained(this)),
              false /* should_record_histograms */)),
      revocation_trial_verifier_(
          net::MultiThreadedCertVerifier::CreateForDualVerificationTrial(
              trial_verify_proc,
              // Unretained is safe since the callback won't be called after
              // |trial_verifier_| is destroyed.
              base::BindRepeating(
                  &TrialComparisonCertVerifier::OnTrialVerifierComplete,
                  base::Unretained(this)),
              false /* should_record_histograms */)),
      weak_ptr_factory_(this) {
  net::CertVerifier::Config config;
  config.enable_rev_checking = true;
  revocation_trial_verifier_->SetConfig(config);
}

TrialComparisonCertVerifier::~TrialComparisonCertVerifier() = default;

// static
void TrialComparisonCertVerifier::SetFakeOfficialBuildForTesting() {
  g_is_fake_official_build_for_cert_verifier_testing = true;
}

int TrialComparisonCertVerifier::Verify(const RequestParams& params,
                                        net::CertVerifyResult* verify_result,
                                        net::CompletionOnceCallback callback,
                                        std::unique_ptr<Request>* out_req,
                                        const net::NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return primary_verifier_->Verify(params, verify_result, std::move(callback),
                                   out_req, net_log);
}

void TrialComparisonCertVerifier::SetConfig(const Config& config) {
  config_ = config;
  config_id_++;

  primary_verifier_->SetConfig(config);
  primary_reverifier_->SetConfig(config);
  trial_verifier_->SetConfig(config);

  // Always enable revocation checking for the revocation trial verifier.
  net::CertVerifier::Config config_with_revocation = config;
  config_with_revocation.enable_rev_checking = true;
  revocation_trial_verifier_->SetConfig(config_with_revocation);

  // Notify all in-process jobs that the underlying configuration has changed.
  for (auto& job : jobs_) {
    job->OnConfigChanged();
  }
}

void TrialComparisonCertVerifier::OnPrimaryVerifierComplete(
    const RequestParams& params,
    const net::NetLogWithSource& net_log,
    int primary_error,
    const net::CertVerifyResult& primary_result,
    base::TimeDelta primary_latency,
    bool is_first_job) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool is_official_build = g_is_fake_official_build_for_cert_verifier_testing;
#if defined(OFFICIAL_BUILD) && defined(GOOGLE_CHROME_BUILD)
  is_official_build = true;
#endif
  if (!is_official_build || !base::FeatureList::IsEnabled(
                                features::kCertDualVerificationTrialFeature)) {
    return;
  }

  base::PostTaskAndReplyWithResult(
      base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
          .get(),
      FROM_HERE, base::BindOnce(CheckTrialEligibility, profile_id_),
      base::BindOnce(&TrialComparisonCertVerifier::MaybeDoTrialVerification,
                     weak_ptr_factory_.GetWeakPtr(), params, net_log,
                     primary_error, primary_result, primary_latency,
                     is_first_job, config_id_, profile_id_));
}

void TrialComparisonCertVerifier::OnTrialVerifierComplete(
    const RequestParams& params,
    const net::NetLogWithSource& net_log,
    int trial_error,
    const net::CertVerifyResult& trial_result,
    base::TimeDelta latency,
    bool is_first_job) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_Job_Latency_TrialSecondary",
                             latency, base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10), 100);
  if (is_first_job) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.CertVerifier_First_Job_Latency_TrialSecondary", latency,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }
}

void TrialComparisonCertVerifier::MaybeDoTrialVerification(
    const RequestParams& params,
    const net::NetLogWithSource& net_log,
    int primary_error,
    const net::CertVerifyResult& primary_result,
    base::TimeDelta primary_latency,
    bool is_first_job,
    uint32_t config_id,
    void* profile_id,
    bool trial_allowed) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If the trial is not allowed, or the configuration has changed while
  // determining if the trial is allowed, no need to continue.
  if (!trial_allowed || config_id != config_id_)
    return;

  // Only record the TrialPrimary histograms for the same set of requests
  // that TrialSecondary histograms will be recorded for, in order to get a
  // direct comparison.
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_Job_Latency_TrialPrimary",
                             primary_latency,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(10), 100);
  if (is_first_job) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.CertVerifier_First_Job_Latency_TrialPrimary", primary_latency,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }

  std::unique_ptr<TrialVerificationJob> job =
      std::make_unique<TrialVerificationJob>(config_, params, net_log, this,
                                             primary_error, primary_result,
                                             profile_id);
  TrialVerificationJob* job_ptr = job.get();
  jobs_.insert(std::move(job));
  job_ptr->Start();
}

void TrialComparisonCertVerifier::RemoveJob(TrialVerificationJob* job_ptr) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = jobs_.find(job_ptr);
  DCHECK(it != jobs_.end());
  jobs_.erase(it);
}
