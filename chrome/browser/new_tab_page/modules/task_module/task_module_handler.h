// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_TASK_MODULE_TASK_MODULE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_TASK_MODULE_TASK_MODULE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/task_module/task_module.mojom.h"
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
  void GetPrimaryTask(GetPrimaryTaskCallback callback) override;
  void DismissTask(const std::string& task_name) override;
  void RestoreTask(const std::string& task_name) override;
  void OnTaskItemClicked(uint32_t index) override;
  void OnRelatedSearchClicked(uint32_t index) override;

 private:
  mojo::Receiver<task_module::mojom::TaskModuleHandler> receiver_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_TASK_MODULE_TASK_MODULE_HANDLER_H_
