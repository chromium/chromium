// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_AUTO_TODOS_STORE_H_
#define CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_AUTO_TODOS_STORE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"

namespace context_hub {

// Abstract base class interface for AutoTodosStore.
class AutoTodosStore {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAutoTodosChanged(
        base::span<const AutoTodoEntry> entries) = 0;
  };

  virtual ~AutoTodosStore() = default;

  using OperationCallback = base::OnceCallback<void(bool success)>;
  using GetAllItemsCallback =
      base::OnceCallback<void(std::vector<AutoTodoEntry>)>;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Adds or replaces a single item in the store.
  // If `item.id` is empty, a unique ID will be assigned.
  virtual void AddOrUpdateItem(AutoTodoEntry item,
                               OperationCallback callback) = 0;

  // Adds or replaces multiple items in the store.
  // If `item.id` is empty, a unique ID will be assigned.
  virtual void AddAllTodos(base::span<const AutoTodoEntry> items,
                           OperationCallback callback) = 0;

  // Deletes a single item by its ID.
  virtual void DeleteItem(const std::string& id,
                          OperationCallback callback) = 0;

  // Deletes a third-party item matching the given tab ID.
  virtual void DeleteItemByTabId(int64_t tab_id,
                                 OperationCallback callback) = 0;

  // Clears all items from the store.
  virtual void Clear(base::OnceClosure callback) = 0;

  // Retrieves all items currently stored in the store.
  virtual void GetAllItems(GetAllItemsCallback callback) const = 0;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_AUTO_TODOS_STORE_H_
