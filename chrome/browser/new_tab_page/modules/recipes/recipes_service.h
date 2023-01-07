// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_SERVICE_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_SERVICE_H_

#include <list>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/new_tab_page/modules/recipes/recipes.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class PrefRegistrySimple;
class Profile;
namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Downloads tasks for current user from GWS.
class RecipesService : public KeyedService {
 public:
  RecipesService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const std::string& application_locale);
  RecipesService(const RecipesService&) = delete;
  ~RecipesService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // KeyedService:
  void Shutdown() override;

  using RecipesCallback =
      base::OnceCallback<void(recipes::mojom::TaskPtr task)>;
  // Downloads and parses tasks and calls |callback| when done.
  // On success |callback| is called with a populated |RecipesData| object
  // of the first task which has not been dismissed. On failure, it is called
  // with nullptr.
  void GetPrimaryTask(RecipesCallback callback);
  // Dismisses the task with the given name and remembers that setting.
  void DismissTask(const std::string& task_name);
  // Restores the task with the given name and remembers that setting.
  void RestoreTask(const std::string& task_name);

 private:
  void OnDataLoaded(network::SimpleURLLoader* loader,
                    RecipesCallback callback,
                    std::unique_ptr<std::string> response);
  void OnJsonParsed(RecipesCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);

  // Returns whether a task with the given name has been dismissed.
  bool IsTaskDismissed(const std::string& task_name);

  raw_ptr<Profile> profile_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::list<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  std::string application_locale_;

  base::WeakPtrFactory<RecipesService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_SERVICE_H_
