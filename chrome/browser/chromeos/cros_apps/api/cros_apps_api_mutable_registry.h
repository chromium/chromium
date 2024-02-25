// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_MUTABLE_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_MUTABLE_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_frame_context.h"
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
  explicit CrosAppsApiMutableRegistry(PassKey passkey, Profile* profile);
  ~CrosAppsApiMutableRegistry() override;

  void AddOrReplaceForTesting(CrosAppsApiInfo api_info);

  // CrosAppsApiRegistry:
  bool CanEnableApi(const CrosAppsApiId api_id) const override;
  std::vector<CrosAppsApiInfo::EnableBlinkRuntimeFeatureFunction>
  GetBlinkFeatureEnablementFunctionsForFrame(
      const CrosAppsApiFrameContext& api_context) const override;
  bool IsApiEnabledForFrame(
      const CrosAppsApiId api_id,
      const CrosAppsApiFrameContext& api_context) const override;

 private:
  bool CanEnableApi(const CrosAppsApiInfo& api_info) const;
  bool IsApiEnabledForFrame(const CrosAppsApiInfo& api_info,
                            const CrosAppsApiFrameContext& api_context) const;

  // The profile `this` is attached to. Safe to retain profile because `this` is
  // owned by the profile.
  const raw_ptr<Profile> profile_;
  base::flat_map<CrosAppsApiId, CrosAppsApiInfo> api_infos_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_MUTABLE_REGISTRY_H_
