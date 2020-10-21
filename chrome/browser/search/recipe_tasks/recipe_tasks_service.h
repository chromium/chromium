// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_SERVICE_H_
#define CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_SERVICE_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/search/recipe_tasks/recipe_tasks.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class PrefRegistrySimple;
class Profile;
namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Downloads recipe tasks for current user from GWS.
class RecipeTasksService : public KeyedService {
 public:
  RecipeTasksService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const std::string& application_locale);
  RecipeTasksService(const RecipeTasksService&) = delete;
  ~RecipeTasksService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // KeyedService:
  void Shutdown() override;

  using RecipeTaskCallback =
      base::OnceCallback<void(recipe_tasks::mojom::RecipeTaskPtr recipe_task)>;
  // Downloads and parses recipe tasks and calls |callback| when done.
  // On success |callback| is called with a populated |RecipeTasksData| object
  // of the first recipe task which has not been dismissed. On failure, it is
  // called with nullptr.
  void GetPrimaryRecipeTask(RecipeTaskCallback callback);
  // Dismisses the task with the given name and remembers that setting.
  void DismissRecipeTask(const std::string& task_name);
  // Restores the task with the given name and remembers that setting.
  void RestoreRecipeTask(const std::string& task_name);

 private:
  void OnDataLoaded(network::SimpleURLLoader* loader,
                    RecipeTaskCallback callback,
                    std::unique_ptr<std::string> response);
  void OnJsonParsed(RecipeTaskCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);

  // Returns whether a task with the given name has been dismissed.
  bool IsRecipeTaskDismissed(const std::string& task_name);

  Profile* profile_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::list<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  std::string application_locale_;

  base::WeakPtrFactory<RecipeTasksService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_SERVICE_H_
