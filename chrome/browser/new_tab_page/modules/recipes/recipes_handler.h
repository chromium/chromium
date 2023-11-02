// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/recipes/recipes.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

// Implementation of the RecipesHandler mojo interface that requests tasks
// from the RecipesService. Instantiated by the NTP upon a connection
// request by a recipe module.
class RecipesHandler : public recipes::mojom::RecipesHandler {
 public:
  RecipesHandler(
      mojo::PendingReceiver<recipes::mojom::RecipesHandler> pending_receiver,
      Profile* profile);
  ~RecipesHandler() override;

  // recipes::mojom::RecipesHandler:
  void GetPrimaryTask(GetPrimaryTaskCallback callback) override;
  void DismissTask(const std::string& task_name) override;
  void RestoreTask(const std::string& task_name) override;
  void OnRecipeClicked(uint32_t index) override;
  void OnRelatedSearchClicked(uint32_t index) override;

 private:
  mojo::Receiver<recipes::mojom::RecipesHandler> receiver_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_HANDLER_H_
