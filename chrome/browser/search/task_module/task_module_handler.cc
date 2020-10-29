// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/task_module/task_module_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/task_module/task_module_service.h"
#include "chrome/browser/search/task_module/task_module_service_factory.h"

namespace {
const char* GetModuleName(task_module::mojom::TaskModuleType task_module_type) {
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      return "RecipeTasks";
    case task_module::mojom::TaskModuleType::kShopping:
      return "ShoppingTasks";
    default:
      NOTREACHED();
  }
}
}  // namespace

TaskModuleHandler::TaskModuleHandler(
    mojo::PendingReceiver<task_module::mojom::TaskModuleHandler>
        pending_receiver,
    Profile* profile)
    : receiver_(this, std::move(pending_receiver)), profile_(profile) {}

TaskModuleHandler::~TaskModuleHandler() = default;

void TaskModuleHandler::GetPrimaryTask(
    task_module::mojom::TaskModuleType task_module_type,
    GetPrimaryTaskCallback callback) {
  TaskModuleServiceFactory::GetForProfile(profile_)->GetPrimaryTask(
      task_module_type, std::move(callback));
}

void TaskModuleHandler::DismissTask(
    task_module::mojom::TaskModuleType task_module_type,
    const std::string& task_name) {
  TaskModuleServiceFactory::GetForProfile(profile_)->DismissTask(
      task_module_type, task_name);
}

void TaskModuleHandler::RestoreTask(
    task_module::mojom::TaskModuleType task_module_type,
    const std::string& task_name) {
  TaskModuleServiceFactory::GetForProfile(profile_)->RestoreTask(
      task_module_type, task_name);
}

void TaskModuleHandler::OnTaskItemClicked(
    task_module::mojom::TaskModuleType task_module_type,
    uint32_t index) {
  std::string task_item_name;
  switch (task_module_type) {
    case task_module::mojom::TaskModuleType::kRecipe:
      task_item_name = "Recipe";
      break;
    case task_module::mojom::TaskModuleType::kShopping:
      task_item_name = "Product";
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramCounts100(
      base::StringPrintf("NewTabPage.%s.%sClick",
                         GetModuleName(task_module_type),
                         task_item_name.c_str()),
      index);
}

void TaskModuleHandler::OnRelatedSearchClicked(
    task_module::mojom::TaskModuleType task_module_type,
    uint32_t index) {
  base::UmaHistogramCounts100(
      base::StringPrintf("NewTabPage.%s.RelatedSearchClick",
                         GetModuleName(task_module_type)),
      index);
}
