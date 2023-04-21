// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_model_provider.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

// Pointer to the global `AppListModelProvider` instance.
AppListModelProvider* g_instance = nullptr;

}  // namespace

AppListModelProvider::AppListModelProvider() {
  DCHECK(!g_instance);
  g_instance = this;
}

AppListModelProvider::~AppListModelProvider() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AppListModelProvider* AppListModelProvider::Get() {
  return g_instance;
}

void AppListModelProvider::SetActiveModel(
    AppListModel* model,
    SearchModel* search_model,
    QuickAppAccessModel* quick_app_access_model) {
  DCHECK(model);
  DCHECK(search_model);
  DCHECK(quick_app_access_model);

  model_ = model;
  search_model_ = search_model;
  quick_app_access_model_ = quick_app_access_model;

  for (auto& observer : observers_)
    observer.OnActiveAppListModelsChanged(model_, search_model_);
}

void AppListModelProvider::ClearActiveModel() {
  model_ = &default_model_;
  search_model_ = &default_search_model_;
  quick_app_access_model_ = &default_quick_app_access_model_;

  for (auto& observer : observers_)
    observer.OnActiveAppListModelsChanged(model_, search_model_);
}

void AppListModelProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppListModelProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
