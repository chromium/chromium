// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace app_list {

// TODO(longbowei): Remove InternalApp related code since it returns an empty
// list.
const std::vector<InternalApp>& GetInternalAppList(const Profile* profile) {
  static base::NoDestructor<std::vector<InternalApp>> internal_app_list_static;
  return *internal_app_list_static;
}

const InternalApp* FindInternalApp(const std::string& app_id) {
  for (const auto& app : GetInternalAppList(nullptr)) {
    if (app_id == app.app_id) {
      return &app;
    }
  }
  return nullptr;
}

bool IsInternalApp(const std::string& app_id) {
  return !!FindInternalApp(app_id);
}

size_t GetNumberOfInternalAppsShowInLauncherForTest(std::string* apps_name,
                                                    const Profile* profile) {
  size_t num_of_internal_apps_show_in_launcher = 0u;
  std::vector<std::string> internal_apps_name;
  for (const auto& app : GetInternalAppList(profile)) {
    if (app.show_in_launcher) {
      ++num_of_internal_apps_show_in_launcher;
      if (apps_name) {
        internal_apps_name.emplace_back(
            l10n_util::GetStringUTF8(app.name_string_resource_id));
      }
    }
  }
  if (apps_name) {
    *apps_name = base::JoinString(internal_apps_name, ",");
  }
  return num_of_internal_apps_show_in_launcher;
}

}  // namespace app_list
