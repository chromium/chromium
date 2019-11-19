// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_EXAMPLE_APP_LIST_CLIENT_H_
#define ASH_SHELL_EXAMPLE_APP_LIST_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/test/test_app_list_client.h"
#include "base/macros.h"

namespace ash {

class AppListControllerImpl;

namespace shell {

class WindowTypeShelfItem;
class ExampleSearchResult;

class ExampleAppListClient : public TestAppListClient {
 public:
  explicit ExampleAppListClient(AppListControllerImpl* controller);
  ~ExampleAppListClient() override;

 private:
  void PopulateApps();
  void DecorateSearchBox();

  // TestAppListClient:
  void StartSearch(const base::string16& trimmed_query) override;
  void OpenSearchResult(const std::string& result_id,
                        int event_flags,
                        ash::AppListLaunchedFrom launched_from,
                        ash::AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override;
  void ActivateItem(int profile_id,
                    const std::string& id,
                    int event_flags) override;

  AppListControllerImpl* controller_;

  std::vector<std::unique_ptr<WindowTypeShelfItem>> apps_;
  std::vector<std::unique_ptr<ExampleSearchResult>> search_results_;

  DISALLOW_COPY_AND_ASSIGN(ExampleAppListClient);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_EXAMPLE_APP_LIST_CLIENT_H_
