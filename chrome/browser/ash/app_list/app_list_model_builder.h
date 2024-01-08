// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "components/sync/protocol/app_list_specifics.pb.h"

class AppListControllerDelegate;
class ChromeAppListItem;
class Profile;

// This abstract class populates and maintains the given |model| with
// information from |profile| for the specific item type.
class AppListModelBuilder {
 public:
  using AppPositionInitCallback =
      base::RepeatingCallback<void(ChromeAppListItem*)>;

  // Sets and resets the callback to initialize a new app's position in tests.
  class ScopedAppPositionInitCallbackForTest {
   public:
    ScopedAppPositionInitCallbackForTest(AppListModelBuilder* builder,
                                         AppPositionInitCallback callback);
    ScopedAppPositionInitCallbackForTest(
        const ScopedAppPositionInitCallbackForTest&) = delete;
    ScopedAppPositionInitCallbackForTest& operator=(
        const ScopedAppPositionInitCallbackForTest) = delete;
    ~ScopedAppPositionInitCallbackForTest();

   private:
    const raw_ptr<AppListModelBuilder> builder_;
    AppPositionInitCallback callback_;
  };

  // |controller| is owned by implementation of AppListService.
  AppListModelBuilder(AppListControllerDelegate* controller,
                      const char* item_type);
  AppListModelBuilder(const AppListModelBuilder&) = delete;
  AppListModelBuilder& operator=(const AppListModelBuilder&) = delete;
  virtual ~AppListModelBuilder();

  // Initialize to use app-list sync and sets |service_| to |service|.
  // |service| is the owner of this instance.
  void Initialize(app_list::AppListSyncableService* service,
                  Profile* profile,
                  AppListModelUpdater* model_updater);

 protected:
  // Builds the model with the current profile.
  virtual void BuildModel() = 0;

  app_list::AppListSyncableService* service() { return service_; }

  Profile* profile() { return profile_; }

  AppListControllerDelegate* controller() { return controller_; }

  AppListModelUpdater* model_updater() { return model_updater_; }

  // Inserts an app based on app ordinal prefs.
  virtual void InsertApp(std::unique_ptr<ChromeAppListItem> app);

  // Removes an app based on app id. If |unsynced_change| is set to true then
  // app is removed only from model and sync service is not used.
  virtual void RemoveApp(const std::string& id, bool unsynced_change);

  // Returns a SyncItem of the specified type if it exists.
  const app_list::AppListSyncableService::SyncItem* GetSyncItem(
      const std::string& id,
      sync_pb::AppListSpecifics::AppListItemType type);

  // Returns app instance matching |id| or nullptr.
  ChromeAppListItem* GetAppItem(const std::string& id);

 private:
  // Unowned pointers to the service that owns this and associated profile.
  raw_ptr<app_list::AppListSyncableService> service_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  // Unowned pointer to an app list model updater.
  raw_ptr<AppListModelUpdater, DanglingUntriaged> model_updater_ = nullptr;

  // Unowned pointer to the app list controller.
  raw_ptr<AppListControllerDelegate, DanglingUntriaged> controller_;

  // Global constant defined for each item type.
  const char* item_type_;

  // The callback to initialize an app's position in tests.
  raw_ptr<AppPositionInitCallback> position_setter_for_test_ = nullptr;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_
