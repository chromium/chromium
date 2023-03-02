// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/networking/policy_cert_service.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/net/nss_temp_certs_cache_chromeos.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_util.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

namespace policy {

PolicyCertService::~PolicyCertService() {
  StopListeningToPolicyCertificateProvider();
}

PolicyCertService::PolicyCertService(
    Profile* profile,
    ash::PolicyCertificateProvider* policy_certificate_provider,
    bool may_use_profile_wide_trust_anchors)
    : profile_(profile),
      policy_certificate_provider_(policy_certificate_provider),
      may_use_profile_wide_trust_anchors_(may_use_profile_wide_trust_anchors) {
  DCHECK(policy_certificate_provider_);
  DCHECK(profile_);

  policy_certificate_provider_->AddPolicyProvidedCertsObserver(this);
  profile_wide_all_server_and_authority_certs_ =
      policy_certificate_provider_->GetAllServerAndAuthorityCertificates(
          chromeos::onc::CertificateScope::Default());
  profile_wide_trust_anchors_ = GetAllowedProfileWideTrustAnchors();
}

PolicyCertService::PolicyCertService(Profile* profile)
    : profile_(profile),
      policy_certificate_provider_(nullptr),
      may_use_profile_wide_trust_anchors_(true) {}

void PolicyCertService::OnPolicyProvidedCertsChanged() {
  profile_wide_all_server_and_authority_certs_ =
      policy_certificate_provider_->GetAllServerAndAuthorityCertificates(
          chromeos::onc::CertificateScope::Default());
  profile_wide_trust_anchors_ = GetAllowedProfileWideTrustAnchors();

  // Make all policy-provided server and authority certificates available to NSS
  // as temp certificates.
  // While the network service is out-of-process so it is not affected by this,
  // this is important for client certificate selection which still happens in
  // the browser process.
  // Note that this is done on the UI thread because the assumption is that NSS
  // has already been initialized by Chrome OS specific start-up code in chrome,
  // expecting that the operation of creating in-memory NSS certs is cheap in
  // that case.
  temp_policy_provided_certs_ =
      std::make_unique<network::NSSTempCertsCacheChromeOS>(
          profile_wide_all_server_and_authority_certs_);

  auto* profile_network_context =
      ProfileNetworkContextServiceFactory::GetForContext(profile_);
  profile_network_context->UpdateAdditionalCertificates();
}

void PolicyCertService::OnPolicyCertificateProviderDestroying() {
  StopListeningToPolicyCertificateProvider();
}

void PolicyCertService::GetPolicyCertificatesForStoragePartition(
    const base::FilePath& partition_path,
    net::CertificateList* out_all_server_and_authority_certificates,
    net::CertificateList* out_trust_anchors) const {
  *out_all_server_and_authority_certificates =
      profile_wide_all_server_and_authority_certs_;
  *out_trust_anchors = profile_wide_trust_anchors_;

  if (policy_certificate_provider_->GetExtensionIdsWithPolicyCertificates()
          .empty()) {
    return;
  }

  // The following code adds policy-provided extension specific certificates.
  // Policy can specify these keyed by extension ID.
  // In general, there is no direct mapping from a StoragePartition path to an
  // extension ID, because extensions could be using the default
  // StoragePartition of the Profile.
  // However, for extensions with isolated storage, the extension will be in a
  // StoragePartition that is exclusively used by this extension.
  // Policy-provided extension specific certificates are thus only allowed for
  // extensions with isolated storage.
  // The following code checks those preconditions and attempts to find the
  // extension ID (among extensions IDs with policy-provided certificates) that
  // corresponds to |partition_path|.

  base::FilePath default_storage_partition_path =
      profile_->GetDefaultStoragePartition()->GetPath();
  // Among the extension IDs that have policy-provided certificates, attempt to
  // find the extension ID which corresponds to |partition_path|.
  // This is done by iterating the extension IDs because there's no trivial
  // conversion from |partition_path| to extension ID as explained above.
  std::string current_extension_id_with_policy_certificates;
  std::set<std::string> extension_ids_with_policy_certificates =
      policy_certificate_provider_->GetExtensionIdsWithPolicyCertificates();
  for (const auto& extension_id : extension_ids_with_policy_certificates) {
    // Only allow policy-provided certificates for extensions with isolated
    // storage. Also sanity-check that it's not the default partition.
    content::StoragePartition* extension_partition =
        extensions::util::GetStoragePartitionForExtensionId(
            extension_id, profile_,
            /*can_create=*/false);
    if (!extension_partition)
      continue;
    if (!extensions::util::HasIsolatedStorage(extension_id, profile_) ||
        extension_partition->GetPath() == default_storage_partition_path) {
      LOG(ERROR) << "Ignoring policy certificates for " << extension_id
                 << " because it does not have isolated storage";
      continue;
    }
    if (partition_path == extension_partition->GetPath()) {
      current_extension_id_with_policy_certificates = extension_id;
      break;
    }
  }

  if (current_extension_id_with_policy_certificates.empty())
    return;

  net::CertificateList extension_all_server_and_authority_certificates =
      policy_certificate_provider_->GetAllServerAndAuthorityCertificates(
          chromeos::onc::CertificateScope::ForExtension(
              current_extension_id_with_policy_certificates));
  out_all_server_and_authority_certificates->insert(
      out_all_server_and_authority_certificates->end(),
      extension_all_server_and_authority_certificates.begin(),
      extension_all_server_and_authority_certificates.end());

  net::CertificateList extension_trust_anchors =
      policy_certificate_provider_->GetWebTrustedCertificates(
          chromeos::onc::CertificateScope::ForExtension(
              current_extension_id_with_policy_certificates));
  out_trust_anchors->insert(out_trust_anchors->end(),
                            extension_trust_anchors.begin(),
                            extension_trust_anchors.end());
}

bool PolicyCertService::UsedPolicyCertificates() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kUsedPolicyCertificates);
}

void PolicyCertService::SetUsedPolicyCertificates() {
  profile_->GetPrefs()->SetBoolean(prefs::kUsedPolicyCertificates, true);
}

net::CertificateList PolicyCertService::GetAllowedProfileWideTrustAnchors() {
  if (!may_use_profile_wide_trust_anchors_)
    return {};

  return policy_certificate_provider_->GetWebTrustedCertificates(
      chromeos::onc::CertificateScope::Default());
}

//  static
void PolicyCertService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kUsedPolicyCertificates, false);
}

// static
std::unique_ptr<PolicyCertService> PolicyCertService::CreateForTesting(
    Profile* profile) {
  return base::WrapUnique(new PolicyCertService(profile));
}

void PolicyCertService::SetPolicyTrustAnchorsForTesting(
    const net::CertificateList& trust_anchors) {
  // Only allow this call in an instance that has been created through
  // PolicyCertService::CreateForTesting.
  CHECK_EQ(nullptr, policy_certificate_provider_);

  profile_wide_all_server_and_authority_certs_ = trust_anchors;
  profile_wide_trust_anchors_ = trust_anchors;
}

void PolicyCertService::StopListeningToPolicyCertificateProvider() {
  if (!policy_certificate_provider_) {
    return;
  }
  policy_certificate_provider_->RemovePolicyProvidedCertsObserver(this);
  policy_certificate_provider_ = nullptr;
}

}  // namespace policy
