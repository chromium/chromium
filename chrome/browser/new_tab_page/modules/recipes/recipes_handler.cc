// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/recipes/recipes_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/new_tab_page/modules/recipes/recipes_service.h"
#include "chrome/browser/new_tab_page/modules/recipes/recipes_service_factory.h"
#include "chrome/browser/profiles/profile.h"

RecipesHandler::RecipesHandler(
    mojo::PendingReceiver<recipes::mojom::RecipesHandler> pending_receiver,
    Profile* profile)
    : receiver_(this, std::move(pending_receiver)), profile_(profile) {}

RecipesHandler::~RecipesHandler() = default;

void RecipesHandler::GetPrimaryTask(GetPrimaryTaskCallback callback) {
  RecipesServiceFactory::GetForProfile(profile_)->GetPrimaryTask(
      std::move(callback));
}

void RecipesHandler::DismissTask(const std::string& task_name) {
  RecipesServiceFactory::GetForProfile(profile_)->DismissTask(task_name);
}

void RecipesHandler::RestoreTask(const std::string& task_name) {
  RecipesServiceFactory::GetForProfile(profile_)->RestoreTask(task_name);
}

void RecipesHandler::OnRecipeClicked(uint32_t index) {
  std::string recipe_name = "Recipe";
  base::UmaHistogramCounts100(
      base::StringPrintf("NewTabPage.RecipeTasks.%sClick", recipe_name.c_str()),
      index);
}

void RecipesHandler::OnRelatedSearchClicked(uint32_t index) {
  base::UmaHistogramCounts100("NewTabPage.RecipeTasks.RelatedSearchClick",
                              index);
}
