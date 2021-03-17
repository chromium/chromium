// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_HANDLER_H_
#define CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_HANDLER_H_

#include "chrome/browser/search/task_module/task_module.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Implementation of the TaskModuleHandler mojo interface that requests tasks
// from the TaskModuleService. Instantiated by the NTP upon a connection request
// by a task module.
class TaskModuleHandler : public task_module::mojom::TaskModuleHandler {
 public:
  TaskModuleHandler(mojo::PendingReceiver<task_module::mojom::TaskModuleHandler>
                        pending_receiver,
                    Profile* profile);
  ~TaskModuleHandler() override;

  // task_module::mojom::TaskModuleHandler:
  void GetPrimaryTask(task_module::mojom::TaskModuleType task_module_type,
                      GetPrimaryTaskCallback callback) override;
  void DismissTask(task_module::mojom::TaskModuleType task_module_type,
                   const std::string& task_name) override;
  void RestoreTask(task_module::mojom::TaskModuleType task_module_type,
                   const std::string& task_name) override;
  void OnTaskItemClicked(task_module::mojom::TaskModuleType task_module_type,
                         uint32_t index) override;
  void OnRelatedSearchClicked(
      task_module::mojom::TaskModuleType task_module_type,
      uint32_t index) override;

 private:
  mojo::Receiver<task_module::mojom::TaskModuleHandler> receiver_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_HANDLER_H_
