// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_SERVICE_H_
#define CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_SERVICE_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/search/shopping_tasks/shopping_tasks.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class PrefRegistrySimple;
class Profile;
namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Downloads shopping tasks for current user from GWS.
class ShoppingTasksService : public KeyedService {
 public:
  ShoppingTasksService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      const std::string& application_locale);
  ShoppingTasksService(const ShoppingTasksService&) = delete;
  ~ShoppingTasksService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // KeyedService:
  void Shutdown() override;

  using ShoppingTaskCallback = base::OnceCallback<void(
      shopping_tasks::mojom::ShoppingTaskPtr shopping_task)>;
  // Downloads and parses shopping tasks and calls |callback| when done.
  // On success |callback| is called with a populated |ShoppingTasksData| object
  // of the first shopping task which has not been dismissed. On failure, it is
  // called with nullptr.
  void GetPrimaryShoppingTask(ShoppingTaskCallback callback);
  // Dismisses the task with the given name and remembers that setting.
  void DismissShoppingTask(const std::string& task_name);
  // Restores the task with the given name and remembers that setting.
  void RestoreShoppingTask(const std::string& task_name);

 private:
  void OnDataLoaded(network::SimpleURLLoader* loader,
                    ShoppingTaskCallback callback,
                    std::unique_ptr<std::string> response);
  void OnJsonParsed(ShoppingTaskCallback callback,
                    data_decoder::DataDecoder::ValueOrError result);

  // Returns whether a task with the given name has been dismissed.
  bool IsShoppingTaskDismissed(const std::string& task_name);

  Profile* profile_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::list<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  std::string application_locale_;

  base::WeakPtrFactory<ShoppingTasksService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_SERVICE_H_
