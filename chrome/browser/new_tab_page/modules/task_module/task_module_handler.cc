// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/task_module/task_module_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/new_tab_page/modules/task_module/task_module_service.h"
#include "chrome/browser/new_tab_page/modules/task_module/task_module_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace {
const char* GetModuleName() {
  return "RecipeTasks";
}
}  // namespace

TaskModuleHandler::TaskModuleHandler(
    mojo::PendingReceiver<task_module::mojom::TaskModuleHandler>
        pending_receiver,
    Profile* profile)
    : receiver_(this, std::move(pending_receiver)), profile_(profile) {}

TaskModuleHandler::~TaskModuleHandler() = default;

void TaskModuleHandler::GetPrimaryTask(GetPrimaryTaskCallback callback) {
  TaskModuleServiceFactory::GetForProfile(profile_)->GetPrimaryTask(
      std::move(callback));
}

void TaskModuleHandler::DismissTask(const std::string& task_name) {
  TaskModuleServiceFactory::GetForProfile(profile_)->DismissTask(task_name);
}

void TaskModuleHandler::RestoreTask(const std::string& task_name) {
  TaskModuleServiceFactory::GetForProfile(profile_)->RestoreTask(task_name);
}

void TaskModuleHandler::OnTaskItemClicked(uint32_t index) {
  std::string task_item_name = "Recipe";
  base::UmaHistogramCounts100(
      base::StringPrintf("NewTabPage.%s.%sClick", GetModuleName(),
                         task_item_name.c_str()),
      index);
}

void TaskModuleHandler::OnRelatedSearchClicked(uint32_t index) {
  base::UmaHistogramCounts100(
      base::StringPrintf("NewTabPage.%s.RelatedSearchClick", GetModuleName()),
      index);
}
