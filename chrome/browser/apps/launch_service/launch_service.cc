// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/launch_service/launch_service.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/extension_app_launch_manager.h"
#include "chrome/browser/apps/launch_service/launch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace apps {

// static
LaunchService* LaunchService::Get(Profile* profile) {
  return LaunchServiceFactory::GetForProfile(profile);
}

LaunchService::LaunchService(Profile* profile) : profile_(profile) {
  DCHECK(profile_);

  // LaunchService must have only one instance in original profile,
  // apart from guest mode where the off-the-record profile is used.
  DCHECK_EQ(profile_->IsGuestSession(), profile_->IsOffTheRecord());

  extension_app_launch_manager_ =
      std::make_unique<ExtensionAppLaunchManager>(profile);
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions) ||
      base::FeatureList::IsEnabled(features::kDesktopPWAsUnifiedLaunch)) {
    web_app_launch_manager_ =
        std::make_unique<web_app::WebAppLaunchManager>(profile);
  } else {
    web_app_launch_manager_ =
        std::make_unique<ExtensionAppLaunchManager>(profile);
  }
}

LaunchService::~LaunchService() {}

LaunchManager& LaunchService::GetLaunchManagerForApp(
    const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetInstalledExtension(
          app_id);
  return (!extension || extension->from_bookmark())
             ? *web_app_launch_manager_
             : *extension_app_launch_manager_;
}

content::WebContents* LaunchService::OpenApplication(
    const AppLaunchParams& params) {
  return GetLaunchManagerForApp(params.app_id).OpenApplication(params);
}

bool LaunchService::OpenApplicationWindow(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory) {
  return GetLaunchManagerForApp(app_id).OpenApplicationWindow(
      app_id, command_line, current_directory);
}

bool LaunchService::OpenApplicationTab(const std::string& app_id) {
  return GetLaunchManagerForApp(app_id).OpenApplicationTab(app_id);
}

}  // namespace apps
