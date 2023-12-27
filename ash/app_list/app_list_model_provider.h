// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_MODEL_PROVIDER_H_
#define ASH_APP_LIST_APP_LIST_MODEL_PROVIDER_H_

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

namespace ash {

// Used by app list views hierarchy to track the active app list model, and the
// app list search model. The active model state is maintained by
// `AppListControllerImpl`, which also serves as an `AppListViewDelegate`.
// Models are owned by ash embedder (chrome), which use `AppListController`
// interface to update active models. Main motivation is effectively handling
// model changes when the active user changes - app list model contains user
// specific data, and the model information shown in the UI should be updated
// whenever the active user changes. This class supports this without a need to
// rebuild the currently used model from scratch.
// Only one instance is expected to exist at a time, and it can be retrieved
// using `AppListModelProvider::Get()`.
class ASH_EXPORT AppListModelProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the active app list model changes.
    virtual void OnActiveAppListModelsChanged(AppListModel* model,
                                              SearchModel* search_model) = 0;
  };

  AppListModelProvider();
  AppListModelProvider(const AppListModelProvider&) = delete;
  AppListModelProvider& operator=(const AppListModelProvider&) = delete;
  ~AppListModelProvider();

  // Returns a global app list model provider. Only one instance is expected to
  // exist at a time. In production and ash based tests, it's the instance owned
  // by `AppListControllerImpl`.
  static AppListModelProvider* Get();

  // Sets active app list and app list search model.
  // NOTE: This method is expected to be primarily called by the class that
  // owns the `AppListModelProvider` instance (in production, that's
  // `AppListControllerImpl`).
  //
  // Both `model` and `search_model` can be null, in which case active models
  // will fallback to default models. This should generally only be done during
  // shutdown.
  void SetActiveModel(AppListModel* model,
                      SearchModel* search_model,
                      QuickAppAccessModel* quick_app_access_model);

  // Resets active app list and search model to the default one.
  void ClearActiveModel();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gets the active app list model.
  // If an active model has not been set, it returns a default model.
  AppListModel* model() { return model_; }

  // Gets the active search model.
  // If an active model has not been set, it returns a default model.
  SearchModel* search_model() { return search_model_; }

  QuickAppAccessModel* quick_app_access_model() {
    return quick_app_access_model_;
  }

 private:
  // Default, empty models that get returned if the provided models are null.
  // Primarily used for convenience, to avoid need for null checks in code that
  // uses app list model, and search model.
  AppListModel default_model_{nullptr};
  SearchModel default_search_model_;
  QuickAppAccessModel default_quick_app_access_model_;

  raw_ptr<AppListModel, DanglingUntriaged> model_ = &default_model_;
  raw_ptr<SearchModel, DanglingUntriaged> search_model_ =
      &default_search_model_;
  raw_ptr<QuickAppAccessModel, DanglingUntriaged> quick_app_access_model_ =
      &default_quick_app_access_model_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_MODEL_PROVIDER_H_
