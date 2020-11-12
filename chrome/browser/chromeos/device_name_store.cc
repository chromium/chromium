// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_store.h"

#include <math.h>

#include "base/rand_util.h"
#include "base/strings/char_traits.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

const size_t kMaxDeviceNameLength = 15;

// Returns a randomly generated device name of the form 'ChromeOS_123456'.
std::string GenerateDeviceName() {
  static constexpr const char* kDeviceNamePrefix = "ChromeOS_";
  constexpr size_t kPrefixLength =
      base::CharTraits<char>::length(kDeviceNamePrefix);
  constexpr size_t kNumDigits = kMaxDeviceNameLength - kPrefixLength;

  // The algorithm below uses the range of integers between 10^n and double
  // that value to create a string of n digits representing the 10^n values in
  // that range while preserving leading zeroes.
  //
  // Example: 3 digits
  // Expected output: 000...999
  // Rand[1000, 1999] -> 1000 -> 1{000} -> "000"
  // Rand[1000, 1999] -> 1782 -> 1{782} -> "782"
  int min = std::pow(10, kNumDigits);
  int max = 2 * min - 1;
  int rand_num = base::RandInt(min, max);
  std::string rand_num_str = base::NumberToString(rand_num).substr(1);
  return kDeviceNamePrefix + rand_num_str;
}

}  // namespace

// static
DeviceNameStore* DeviceNameStore::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::Singleton<DeviceNameStore>::get();
}

// static
void DeviceNameStore::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  DCHECK(registry);
  registry->RegisterStringPref(prefs::kDeviceName, "");
}

void DeviceNameStore::Initialize(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs);
  prefs_ = prefs;

  std::string device_name = prefs_->GetString(prefs::kDeviceName);
  if (device_name.empty()) {
    device_name = GenerateDeviceName();
    prefs_->SetString(prefs::kDeviceName, device_name);
  }
}

std::string DeviceNameStore::GetDeviceName() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(prefs_);
  return prefs_->GetString(prefs::kDeviceName);
}

}  // namespace chromeos
