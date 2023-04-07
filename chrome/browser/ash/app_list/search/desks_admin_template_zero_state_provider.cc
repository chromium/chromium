// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/desks_admin_template_zero_state_provider.h"

#include <memory>

#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {
namespace {

// A hardcode id for each admin template.
constexpr char kAdminTemplateResultPrefix[] = "admin-template://";

}  // namespace

DesksAdminTemplateZeroStateResult::DesksAdminTemplateZeroStateResult(
    Profile* profile,
    const base ::GUID& template_uuid,
    const std::u16string& title,
    const gfx::ImageSkia& icon)
    : profile_(profile), template_uuid_(template_uuid) {
  DCHECK(profile_);
  set_id(kAdminTemplateResultPrefix + template_uuid.AsLowercaseString());
  SetCategory(Category::kUnknown);
  SetTitle(title);
  SetDetails(u"Managed by administrator");
  SetResultType(ResultType::kZeroStateDesksAdminTemplate);
  SetDisplayType(DisplayType::kContinue);
  SetMetricsType(ash::ZERO_STATE_DESKS_ADMIN_TEMPLATE);
  SetChipIcon(icon);
}

DesksAdminTemplateZeroStateResult::~DesksAdminTemplateZeroStateResult() =
    default;

void DesksAdminTemplateZeroStateResult::Open(int event_flags) {
  ash::SavedDeskController::Get()->LaunchAdminTemplate(template_uuid_);
}

DesksAdminTemplateZeroStateProvider::DesksAdminTemplateZeroStateProvider(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

DesksAdminTemplateZeroStateProvider::~DesksAdminTemplateZeroStateProvider() =
    default;

void DesksAdminTemplateZeroStateProvider::StartZeroState() {
  SearchProvider::Results search_results;

  auto* controller = ash::SavedDeskController::Get();

  auto* color_provider = ash::ColorProvider::Get();
  // NOTE: Color provider may not be set in unit tests.
  SkColor icon_color =
      color_provider
          ? color_provider->GetContentLayerColor(
                ash::ColorProvider::ContentLayerType::kButtonIconColorPrimary)
          : gfx::kGoogleGrey900;
  // TODO(b/273799604): We will replace the default icon with our admin
  // templates icon.
  gfx::ImageSkia icon = gfx::CreateVectorIcon(
      ash::kReleaseNotesChipIcon, app_list::kSystemIconDimension, icon_color);

  for (const auto& metadata : controller->GetAdminTemplateMetadata()) {
    // With productivity launcher enabled, release notes are shown in continue
    // section.
    search_results.emplace_back(
        std::make_unique<DesksAdminTemplateZeroStateResult>(
            profile_, metadata.uuid, metadata.name, icon));
  }

  SwapResults(&search_results);
}

ash::AppListSearchResultType DesksAdminTemplateZeroStateProvider::ResultType()
    const {
  return ash::AppListSearchResultType::kZeroStateDesksAdminTemplate;
}

}  // namespace app_list
