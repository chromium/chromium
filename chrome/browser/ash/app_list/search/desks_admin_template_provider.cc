// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/desks_admin_template_provider.h"

#include <memory>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {
namespace {

// A hardcode id for each admin template.
constexpr char kAdminTemplateResultPrefix[] = "admin-template://";

}  // namespace

DesksAdminTemplateResult::DesksAdminTemplateResult(
    Profile* profile,
    AppListControllerDelegate* list_controller,
    const base ::Uuid& template_uuid,
    const std::u16string& title,
    const ui::ImageModel& icon)
    : profile_(profile),
      list_controller_(list_controller),
      template_uuid_(template_uuid) {
  DCHECK(profile_);
  set_id(kAdminTemplateResultPrefix + template_uuid.AsLowercaseString());
  SetCategory(Category::kUnknown);
  SetTitle(title);
  SetDetails(u"Managed by administrator");
  SetResultType(ResultType::kDesksAdminTemplate);
  SetDisplayType(DisplayType::kContinue);
  SetMetricsType(ash::DESKS_ADMIN_TEMPLATE);
  SetIcon(IconInfo(icon, kSystemIconDimension));
}

DesksAdminTemplateResult::~DesksAdminTemplateResult() = default;

void DesksAdminTemplateResult::Open(int event_flags) {
  ash::SavedDeskController::Get()->LaunchAdminTemplate(
      template_uuid_, list_controller_->GetAppListDisplayId());
}

DesksAdminTemplateProvider::DesksAdminTemplateProvider(
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : SearchProvider(SearchCategory::kDesksAdmin),
      profile_(profile),
      list_controller_(list_controller) {
  DCHECK(profile_);
}

DesksAdminTemplateProvider::~DesksAdminTemplateProvider() = default;

void DesksAdminTemplateProvider::StartZeroState() {
  SearchProvider::Results search_results;

  auto* controller = ash::SavedDeskController::Get();

  ui::ColorId color_id = cros_tokens::kCrosSysTertiary;
  ui::ImageModel icon = ui::ImageModel::FromVectorIcon(
      ash::kDesksTemplatesIcon, color_id, kSystemIconDimension);

  for (const auto& metadata : controller->GetAdminTemplateMetadata()) {
    // Release notes are shown in the Continue section.
    search_results.emplace_back(std::make_unique<DesksAdminTemplateResult>(
        profile_, list_controller_, metadata.uuid, metadata.name, icon));
  }

  SwapResults(&search_results);
}

ash::AppListSearchResultType DesksAdminTemplateProvider::ResultType() const {
  return ash::AppListSearchResultType::kDesksAdminTemplate;
}

}  // namespace app_list
