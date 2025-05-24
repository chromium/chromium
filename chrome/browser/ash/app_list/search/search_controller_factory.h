// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_FACTORY_H_

#include <memory>

#include "chrome/browser/ash/app_list/app_list_model_updater.h"

class AppListControllerDelegate;
class Profile;

namespace ash {
class AppListNotifier;

namespace federated {
class FederatedServiceController;
}  // namespace federated

}  // namespace ash

namespace app_list {

class SearchController;

// Build a SearchController instance with the profile.
std::unique_ptr<SearchController> CreateSearchController(
    Profile* profile,
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    ash::AppListNotifier* notifier,
    ash::federated::FederatedServiceController* federated_service_controller);

// Returns a bitmask of `AutocompleteProvider::Type` for Launcher's
// `SearchController`.
int LauncherSearchProviderTypes();

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_FACTORY_H_
