// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_DESKS_ADMIN_TEMPLATE_ZERO_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_DESKS_ADMIN_TEMPLATE_ZERO_STATE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

class Profile;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace app_list {

// Search results for admin templates.
class DesksAdminTemplateZeroStateResult : public ChromeSearchResult {
 public:
  DesksAdminTemplateZeroStateResult(Profile* profile,
                                    const base::GUID& template_uuid,
                                    const std::u16string& title,
                                    const gfx::ImageSkia& icon);

  ~DesksAdminTemplateZeroStateResult() override;

  DesksAdminTemplateZeroStateResult(const DesksAdminTemplateZeroStateResult&) =
      delete;
  DesksAdminTemplateZeroStateResult& operator=(
      const DesksAdminTemplateZeroStateResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

 private:
  Profile* const profile_;
  base::GUID template_uuid_;
};

// Provides zero-state results from the admin templates. The admin template is a
// new “type” of desk templates that users will launch from the app launcher.
// The admin templates will appear in the continue section view.
class DesksAdminTemplateZeroStateProvider : public SearchProvider {
 public:
  explicit DesksAdminTemplateZeroStateProvider(Profile* profile);
  ~DesksAdminTemplateZeroStateProvider() override;

  DesksAdminTemplateZeroStateProvider(
      const DesksAdminTemplateZeroStateProvider&) = delete;
  DesksAdminTemplateZeroStateProvider& operator=(
      const DesksAdminTemplateZeroStateProvider&) = delete;

  // SearchProvider:
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  Profile* const profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_DESKS_ADMIN_TEMPLATE_ZERO_STATE_PROVIDER_H_
