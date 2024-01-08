// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_DESKS_ADMIN_TEMPLATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_DESKS_ADMIN_TEMPLATE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

class AppListControllerDelegate;
class Profile;

namespace ui {
class ImageModel;
}  // namespace ui

namespace app_list {

// Search results for admin templates.
class DesksAdminTemplateResult : public ChromeSearchResult {
 public:
  DesksAdminTemplateResult(Profile* profile,
                           AppListControllerDelegate* list_controller,
                           const base::Uuid& template_uuid,
                           const std::u16string& title,
                           const ui::ImageModel& icon);

  ~DesksAdminTemplateResult() override;

  DesksAdminTemplateResult(const DesksAdminTemplateResult&) = delete;
  DesksAdminTemplateResult& operator=(const DesksAdminTemplateResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<AppListControllerDelegate> list_controller_;
  base::Uuid template_uuid_;
};

// Provides search results from the admin templates. The admin template is a new
// “type” of desk templates that users will launch from the app launcher. The
// admin templates will appear in the continue section view.
class DesksAdminTemplateProvider : public SearchProvider {
 public:
  DesksAdminTemplateProvider(Profile* profile,
                             AppListControllerDelegate* list_controller);
  ~DesksAdminTemplateProvider() override;

  DesksAdminTemplateProvider(const DesksAdminTemplateProvider&) = delete;
  DesksAdminTemplateProvider& operator=(const DesksAdminTemplateProvider&) =
      delete;

  // SearchProvider:
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<AppListControllerDelegate> list_controller_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_DESKS_ADMIN_TEMPLATE_PROVIDER_H_
