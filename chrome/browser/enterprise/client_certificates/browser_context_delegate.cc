// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/browser_context_delegate.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace client_certificates {

namespace {

constexpr char kLoggingContext[] = "Browser";

}  // namespace

BrowserContextDelegate::BrowserContextDelegate(PrefService* local_state)
    : local_state_(local_state) {
  CHECK(local_state_);
}

BrowserContextDelegate::~BrowserContextDelegate() = default;

void BrowserContextDelegate::OnClientCertificateDeleted(
    scoped_refptr<net::X509Certificate> certificate) {
  if (!g_browser_process || !g_browser_process->profile_manager()) {
    return;
  }

  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    profile->ForEachLoadedStoragePartition(
        [&](content::StoragePartition* storage_partition) {
          storage_partition->GetNetworkContext()->FlushMatchingCachedClientCert(
              certificate);
        });
  }
}

std::string BrowserContextDelegate::GetExpectedCommonName() const {
  if (std::optional<std::string> computer_name =
          GetConfiguredComputerName(local_state_)) {
    return *computer_name;
  }
  return policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
}

std::string BrowserContextDelegate::GetIdentityName() {
  return kManagedBrowserIdentityName;
}

std::string BrowserContextDelegate::GetTemporaryIdentityName() {
  return kTemporaryManagedBrowserIdentityName;
}

std::string BrowserContextDelegate::GetPolicyPref() {
  return prefs::kProvisionManagedClientCertificateForBrowserPrefs;
}

std::string BrowserContextDelegate::GetConfigPolicyPref() const {
  return prefs::kManagedClientCertificateForBrowserConfigPrefs;
}

std::string BrowserContextDelegate::GetLoggingContext() {
  return kLoggingContext;
}

}  // namespace client_certificates
