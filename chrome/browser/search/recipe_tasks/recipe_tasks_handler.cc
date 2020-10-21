// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/recipe_tasks/recipe_tasks_handler.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/recipe_tasks/recipe_tasks_service.h"
#include "chrome/browser/search/recipe_tasks/recipe_tasks_service_factory.h"

RecipeTasksHandler::RecipeTasksHandler(
    mojo::PendingReceiver<recipe_tasks::mojom::RecipeTasksHandler>
        pending_receiver,
    Profile* profile)
    : receiver_(this, std::move(pending_receiver)), profile_(profile) {}

RecipeTasksHandler::~RecipeTasksHandler() = default;

void RecipeTasksHandler::GetPrimaryRecipeTask(
    GetPrimaryRecipeTaskCallback callback) {
  RecipeTasksServiceFactory::GetForProfile(profile_)->GetPrimaryRecipeTask(
      std::move(callback));
}

void RecipeTasksHandler::DismissRecipeTask(const std::string& task_name) {
  RecipeTasksServiceFactory::GetForProfile(profile_)->DismissRecipeTask(
      task_name);
}

void RecipeTasksHandler::RestoreRecipeTask(const std::string& task_name) {
  RecipeTasksServiceFactory::GetForProfile(profile_)->RestoreRecipeTask(
      task_name);
}

void RecipeTasksHandler::OnRecipeClicked(uint32_t index) {
  base::UmaHistogramCounts100("NewTabPage.RecipeTasks.RecipeClick", index);
}

void RecipeTasksHandler::OnRelatedSearchClicked(uint32_t index) {
  base::UmaHistogramCounts100("NewTabPage.RecipeTasks.RelatedSearchClick",
                              index);
}
