// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/shopping_tasks/shopping_tasks_handler.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/shopping_tasks/shopping_tasks_service.h"
#include "chrome/browser/search/shopping_tasks/shopping_tasks_service_factory.h"

ShoppingTasksHandler::ShoppingTasksHandler(
    mojo::PendingReceiver<shopping_tasks::mojom::ShoppingTasksHandler>
        pending_receiver,
    Profile* profile)
    : receiver_(this, std::move(pending_receiver)), profile_(profile) {}

ShoppingTasksHandler::~ShoppingTasksHandler() = default;

void ShoppingTasksHandler::GetPrimaryShoppingTask(
    GetPrimaryShoppingTaskCallback callback) {
  ShoppingTasksServiceFactory::GetForProfile(profile_)->GetPrimaryShoppingTask(
      std::move(callback));
}

void ShoppingTasksHandler::DismissShoppingTask(const std::string& task_name) {
  ShoppingTasksServiceFactory::GetForProfile(profile_)->DismissShoppingTask(
      task_name);
}

void ShoppingTasksHandler::RestoreShoppingTask(const std::string& task_name) {
  ShoppingTasksServiceFactory::GetForProfile(profile_)->RestoreShoppingTask(
      task_name);
}

void ShoppingTasksHandler::OnProductClicked(uint32_t index) {
  base::UmaHistogramCounts100("NewTabPage.ShoppingTasks.ProductClick", index);
}

void ShoppingTasksHandler::OnRelatedSearchClicked(uint32_t index) {
  base::UmaHistogramCounts100("NewTabPage.ShoppingTasks.RelatedSearchClick",
                              index);
}
