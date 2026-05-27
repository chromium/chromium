// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/platform_management_status_provider_android.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/util/android_enterprise_info.h"
#include "components/policy/core/common/policy_pref_names.h"

#if BUILDFLAG(IS_ANDROID)

namespace policy {

AndroidManagementStatusProvider::AndroidManagementStatusProvider()
    : ManagementStatusProvider(policy_prefs::kEnterpriseMDMManagementAndroid) {}

AndroidManagementStatusProvider::~AndroidManagementStatusProvider() = default;

EnterpriseManagementAuthority AndroidManagementStatusProvider::FetchAuthority() {
  NOTREACHED();
}

void AndroidManagementStatusProvider::FetchAuthorityAsync(
    base::OnceCallback<void(std::pair<ManagementStatusProvider*,
                                      EnterpriseManagementAuthority>)>
        callback) {
  enterprise_util::AndroidEnterpriseInfo::GetInstance()
      ->GetAndroidEnterpriseInfoState(
          base::BindOnce(&AndroidManagementStatusProvider::
                             OnAndroidOwnedStateCheckComplete,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AndroidManagementStatusProvider::OnAndroidOwnedStateCheckComplete(
    base::OnceCallback<void(std::pair<ManagementStatusProvider*,
                                      EnterpriseManagementAuthority>)>
        callback,
    bool has_device_owner,
    bool has_profile_owner) {
  EnterpriseManagementAuthority authority = EnterpriseManagementAuthority::NONE;
  if (has_device_owner || has_profile_owner) {
    authority = EnterpriseManagementAuthority::CLOUD;
  }

  std::move(callback).Run({this, authority});
}

}  // namespace policy

#endif  // BUILDFLAG(IS_ANDROID)
