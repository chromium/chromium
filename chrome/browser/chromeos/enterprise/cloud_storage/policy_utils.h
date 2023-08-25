// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_POLICY_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_POLICY_UTILS_H_

class PrefRegistrySimple;

namespace chromeos::cloud_storage {
void RegisterProfilePrefs(PrefRegistrySimple* registry);
}  // namespace chromeos::cloud_storage

#endif  // CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_POLICY_UTILS_H_
