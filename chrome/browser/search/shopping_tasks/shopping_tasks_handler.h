// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_HANDLER_H_
#define CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_HANDLER_H_

#include "chrome/browser/search/shopping_tasks/shopping_tasks.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Implementation of the ShoppingTasksHandler mojo interface that requests
// shopping tasks from the ShoppingTasksService. Instantiated by the NTP upon a
// connection request by the shopping tasks module.
class ShoppingTasksHandler
    : public shopping_tasks::mojom::ShoppingTasksHandler {
 public:
  ShoppingTasksHandler(
      mojo::PendingReceiver<shopping_tasks::mojom::ShoppingTasksHandler>
          pending_receiver,
      Profile* profile);
  ~ShoppingTasksHandler() override;

  // shopping_tasks::mojom::ShoppingTasksHandler:
  void GetPrimaryShoppingTask(GetPrimaryShoppingTaskCallback callback) override;
  void DismissShoppingTask(const std::string& task_name) override;
  void RestoreShoppingTask(const std::string& task_name) override;
  void OnProductClicked(uint32_t index) override;
  void OnRelatedSearchClicked(uint32_t index) override;

 private:
  mojo::Receiver<shopping_tasks::mojom::ShoppingTasksHandler> receiver_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_HANDLER_H_
