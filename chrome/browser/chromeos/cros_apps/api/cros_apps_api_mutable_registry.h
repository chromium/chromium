// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_MUTABLE_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_MUTABLE_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_info.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_registry.h"

class Profile;
class CrosAppsApiInfo;

// CrosAppsApiMutableRegistry stores and maintains information about ChromeOS
// Apps APIs, and implements the CrosAppsApiRegistry interface.
//
// Most caller should use CrosAppsApiRegistry instead. This class is intended
// for callers that really need to modify the API registry (e.g. performing
// browsertest setup).
class CrosAppsApiMutableRegistry : public CrosAppsApiRegistry,
                                   public base::SupportsUserData::Data {
 public:
  // See CrosAppsApiRegistry::GetInstance() about lifetime.
  static CrosAppsApiMutableRegistry& GetInstance(Profile* profile);

  using PassKey = base::PassKey<CrosAppsApiMutableRegistry>;
  explicit CrosAppsApiMutableRegistry(PassKey passkey);
  ~CrosAppsApiMutableRegistry() override;

  void AddOrReplaceForTesting(CrosAppsApiInfo api_info);

  // CrosAppsApiRegistry:
  std::vector<CrosAppsApiInfo::EnableBlinkRuntimeFeatureFunction>
  GetBlinkFeatureEnablementFunctionsFor(
      content::NavigationHandle* navigation_handle) const override;

 private:
  bool IsApiEnabledFor(const CrosAppsApiInfo& api_info,
                       content::NavigationHandle* navigation_handle) const;

  base::flat_map<blink::mojom::RuntimeFeature, CrosAppsApiInfo> apis_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_MUTABLE_REGISTRY_H_
