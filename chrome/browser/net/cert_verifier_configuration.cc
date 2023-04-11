// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/cert_verifier_configuration.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"

namespace {

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
bool ShouldUseChromeRootStore(PrefService* local_state) {
#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
  // `local_state` should exist when this is called in the browser, but may be
  // null in unit_tests.
  if (local_state) {
    const PrefService::Preference* chrome_root_store_enabled_pref =
        local_state->FindPreference(prefs::kChromeRootStoreEnabled);
    if (chrome_root_store_enabled_pref->IsManaged())
      return chrome_root_store_enabled_pref->GetValue()->GetBool();
  }
#endif  // BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
  return base::FeatureList::IsEnabled(net::features::kChromeRootStoreUsed);
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)

// Calculates and caches the CertVerifierServiceParams so that all calls to
// GetChromeCertVerifierServiceParams will return the same params. The
// params are controllable by enterprise policies which can change during
// runtime, but dynamic updates are not supported, since changing the value
// would not update any existing verifiers that had already been created.
//
// Aside from just being confusing, there are some implementations where
// creating multiple configurations of the verifier in the same process is not
// possible. (For example, using the NSS trust anchors requires a shared
// library to be loaded, while another configuration that doesn't want to use
// the NSS trust anchors may require that library *not* be loaded. See
// https://crbug.com/1340420.)
class CertVerifierServiceConfigurationStorage {
 public:
  explicit CertVerifierServiceConfigurationStorage(PrefService* local_state) {
    params_ = cert_verifier::mojom::CertVerifierServiceParams::New();

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
    if (!local_state) {
      local_state = g_browser_process->local_state();
    }
    params_->use_chrome_root_store = ShouldUseChromeRootStore(local_state);
#endif
  }
  ~CertVerifierServiceConfigurationStorage() = delete;

  cert_verifier::mojom::CertVerifierServiceParamsPtr Params() const {
    return params_.Clone();
  }

 private:
  cert_verifier::mojom::CertVerifierServiceParamsPtr params_;
};

}  // namespace

cert_verifier::mojom::CertVerifierServiceParamsPtr
GetChromeCertVerifierServiceParams(PrefService* local_state) {
  static base::NoDestructor<CertVerifierServiceConfigurationStorage> storage(
      local_state);

  return storage->Params();
}
