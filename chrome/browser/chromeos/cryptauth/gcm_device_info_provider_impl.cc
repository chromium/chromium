// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cryptauth/gcm_device_info_provider_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/hash/md5.h"
#include "base/linux_util.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/cryptauth/cryptauth_device_id_provider_impl.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/version_info/version_info.h"

namespace chromeos {

namespace {

int64_t HashStringToInt64(const std::string& string) {
  base::MD5Context context;
  base::MD5Init(&context);
  base::MD5Update(&context, string);

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);

  // Fold the digest into an int64_t value. |digest.a| is a 16-byte array, so we
  // sum the two 8-byte halves of the digest to create the hash.
  int64_t hash = 0;
  for (size_t i = 0; i < sizeof(digest.a); ++i) {
    uint8_t byte = digest.a[i];
    hash += static_cast<int64_t>(byte) << (i % sizeof(int64_t));
  }

  return hash;
}

}  // namespace

// static
const GcmDeviceInfoProviderImpl* GcmDeviceInfoProviderImpl::GetInstance() {
  static const base::NoDestructor<GcmDeviceInfoProviderImpl> provider;
  return provider.get();
}

const cryptauth::GcmDeviceInfo& GcmDeviceInfoProviderImpl::GetGcmDeviceInfo()
    const {
  static const base::NoDestructor<cryptauth::GcmDeviceInfo> gcm_device_info([] {
    static const google::protobuf::int64 kSoftwareVersionCode =
        HashStringToInt64(version_info::GetLastChange());

    cryptauth::GcmDeviceInfo gcm_device_info;

    gcm_device_info.set_long_device_id(
        cryptauth::CryptAuthDeviceIdProviderImpl::GetInstance()->GetDeviceId());
    gcm_device_info.set_device_type(cryptauth::CHROME);
    gcm_device_info.set_device_software_version(
        version_info::GetVersionNumber());
    gcm_device_info.set_device_software_version_code(kSoftwareVersionCode);
    gcm_device_info.set_locale(
        ChromeContentBrowserClient().GetApplicationLocale());
    gcm_device_info.set_device_model(base::SysInfo::GetLsbReleaseBoard());
    gcm_device_info.set_device_os_version(base::GetLinuxDistro());
    // The Chrome OS version tracks the Chrome version, so fill in the same
    // value as |device_kSoftwareVersionCode|.
    gcm_device_info.set_device_os_version_code(kSoftwareVersionCode);
    // |device_display_diagonal_mils| is unused because it only applies to
    // phones/tablets, but it must be set due to server API verification.
    gcm_device_info.set_device_display_diagonal_mils(0);

    // Smart Lock, MultiDevice Setup and Messages are supported on all
    // Chromebooks.
    gcm_device_info.add_supported_software_features(
        cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT);
    gcm_device_info.add_supported_software_features(
        cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT);
    gcm_device_info.add_supported_software_features(
        cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT);

    // Instant Tethering is only supported if the associated flag is enabled.
    if (base::FeatureList::IsEnabled(features::kInstantTethering)) {
      gcm_device_info.add_supported_software_features(
          cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT);
    }

    return gcm_device_info;
  }());

  return *gcm_device_info;
}

GcmDeviceInfoProviderImpl::GcmDeviceInfoProviderImpl() = default;

}  // namespace chromeos
