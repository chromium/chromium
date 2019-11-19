// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_metrics.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/extensions/default_web_app_ids.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "extensions/common/constants.h"

namespace {

// The default Essential app's histogram name. This is used for logging so do
// not change the order of this enum.
// https://docs.google.com/document/d/1WJ-BjlVOM87ygIsdDBCyXxdKw3iS5EtNGm1fWiWhfIs
enum class DefaultAppName {
  kCalculator = 10,
  kText = 11,
  kGetHelp = 12,
  kGallery = 13,
  kVideoPlayer = 14,
  kAudioPlayer = 15,
  kChromeCanvas = 16,
  kCamera = 17,
  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kCamera,
};

void RecordDefaultAppLaunch(DefaultAppName default_app_name,
                            apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      return;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      UMA_HISTOGRAM_ENUMERATION("Apps.DefaultAppLaunch.FromAppListGrid",
                                default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.DefaultAppLaunch.FromAppListGridContextMenu", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      UMA_HISTOGRAM_ENUMERATION("Apps.DefaultAppLaunch.FromAppListQuery",
                                default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.DefaultAppLaunch.FromAppListQueryContextMenu",
          default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      UMA_HISTOGRAM_ENUMERATION(
          "Apps.DefaultAppLaunch.FromAppListRecommendation", default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromShelf:
      UMA_HISTOGRAM_ENUMERATION("Apps.DefaultAppLaunch.FromShelf",
                                default_app_name);
      break;
    case apps::mojom::LaunchSource::kFromFileManager:
      UMA_HISTOGRAM_ENUMERATION("Apps.DefaultAppLaunch.FromFileManager",
                                default_app_name);
      break;
    // TODO(crbug.com/853604): Add metrics.
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
      return;
  }
}

void RecordBuiltInAppLaunch(apps::BuiltInAppName built_in_app_name,
                            apps::mojom::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
    case apps::mojom::LaunchSource::kFromParentalControls:
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      UMA_HISTOGRAM_ENUMERATION("Apps.AppListInternalApp.Activate",
                                built_in_app_name);
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchResultInternalApp.Open",
                                built_in_app_name);
      break;
    case apps::mojom::LaunchSource::kFromShelf:
    case apps::mojom::LaunchSource::kFromFileManager:
    case apps::mojom::LaunchSource::kFromLink:
    case apps::mojom::LaunchSource::kFromOmnibox:
      break;
  }
}

}  // namespace

namespace apps {

void RecordAppLaunch(const std::string& app_id,
                     apps::mojom::LaunchSource launch_source) {
  if (app_id == extension_misc::kCalculatorAppId)
    RecordDefaultAppLaunch(DefaultAppName::kCalculator, launch_source);
  else if (app_id == extension_misc::kTextEditorAppId)
    RecordDefaultAppLaunch(DefaultAppName::kText, launch_source);
  else if (app_id == extension_misc::kGeniusAppId)
    RecordDefaultAppLaunch(DefaultAppName::kGetHelp, launch_source);
  else if (app_id == file_manager::kGalleryAppId)
    RecordDefaultAppLaunch(DefaultAppName::kGallery, launch_source);
  else if (app_id == file_manager::kVideoPlayerAppId)
    RecordDefaultAppLaunch(DefaultAppName::kVideoPlayer, launch_source);
  else if (app_id == file_manager::kAudioPlayerAppId)
    RecordDefaultAppLaunch(DefaultAppName::kAudioPlayer, launch_source);
  else if (app_id == chromeos::default_web_apps::kCanvasAppId)
    RecordDefaultAppLaunch(DefaultAppName::kChromeCanvas, launch_source);
  else if (app_id == extension_misc::kCameraAppId)
    RecordDefaultAppLaunch(DefaultAppName::kCamera, launch_source);

  // Above are default Essential apps; below are built-in apps.

  else if (app_id == ash::kInternalAppIdKeyboardShortcutViewer)
    RecordBuiltInAppLaunch(BuiltInAppName::kKeyboardShortcutViewer,
                           launch_source);
  else if (app_id == ash::kReleaseNotesAppId)
    RecordBuiltInAppLaunch(BuiltInAppName::kReleaseNotes, launch_source);
  else if (app_id == ash::kInternalAppIdCamera)
    RecordBuiltInAppLaunch(BuiltInAppName::kCamera, launch_source);
  else if (app_id == ash::kInternalAppIdDiscover)
    RecordBuiltInAppLaunch(BuiltInAppName::kDiscover, launch_source);
  else if (app_id == ash::kInternalAppIdSettings)
    RecordBuiltInAppLaunch(BuiltInAppName::kSettings, launch_source);
  else if (app_id == plugin_vm::kPluginVmAppId)
    RecordBuiltInAppLaunch(BuiltInAppName::kPluginVm, launch_source);
}

void RecordBuiltInAppSearchResult(const std::string& app_id) {
  if (app_id == ash::kInternalAppIdKeyboardShortcutViewer) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchResultInternalApp.Show",
                              BuiltInAppName::kKeyboardShortcutViewer);
  } else if (app_id == ash::kReleaseNotesAppId) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchResultInternalApp.Show",
                              BuiltInAppName::kReleaseNotes);
  } else if (app_id == ash::kInternalAppIdCamera) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchResultInternalApp.Show",
                              BuiltInAppName::kCamera);
  } else if (app_id == ash::kInternalAppIdDiscover) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchResultInternalApp.Show",
                              BuiltInAppName::kDiscover);
  } else if (app_id == ash::kInternalAppIdSettings) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchResultInternalApp.Show",
                              BuiltInAppName::kSettings);
  } else if (app_id == plugin_vm::kPluginVmAppId) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchResultInternalApp.Show",
                              BuiltInAppName::kPluginVm);
  }
}

}  // namespace apps
