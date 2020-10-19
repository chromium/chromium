// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_HANDLER_H_
#define CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_HANDLER_H_

#include "chrome/browser/search/recipe_tasks/recipe_tasks.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Implementation of the RecipeTasksHandler mojo interface that requests
// recipe tasks from the RecipeTasksService. Instantiated by the NTP upon a
// connection request by the recipe tasks module.
class RecipeTasksHandler : public recipe_tasks::mojom::RecipeTasksHandler {
 public:
  RecipeTasksHandler(
      mojo::PendingReceiver<recipe_tasks::mojom::RecipeTasksHandler>
          pending_receiver,
      Profile* profile);
  ~RecipeTasksHandler() override;

  // recipe_tasks::mojom::RecipeTasksHandler:
  void GetPrimaryRecipeTask(GetPrimaryRecipeTaskCallback callback) override;
  void DismissRecipeTask(const std::string& task_name) override;
  void RestoreRecipeTask(const std::string& task_name) override;
  void OnRecipeClicked(uint32_t index) override;
  void OnRelatedSearchClicked(uint32_t index) override;

 private:
  mojo::Receiver<recipe_tasks::mojom::RecipeTasksHandler> receiver_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_HANDLER_H_
