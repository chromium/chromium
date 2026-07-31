// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_IN_MEMORY_AUTO_TODOS_STORE_H_
#define CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_IN_MEMORY_AUTO_TODOS_STORE_H_

#include <cstdint>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/observer_list.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/context_hub/auto_todos/auto_todos_store.h"

namespace context_hub {

// In-memory implementation of AutoTodosStore backed by base::LRUCache.
class InMemoryAutoTodosStore : public AutoTodosStore {
 public:
  InMemoryAutoTodosStore();
  InMemoryAutoTodosStore(const InMemoryAutoTodosStore&) = delete;
  InMemoryAutoTodosStore& operator=(const InMemoryAutoTodosStore&) = delete;
  ~InMemoryAutoTodosStore() override;

  // AutoTodosStore:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void AddOrUpdateItem(AutoTodoEntry item, OperationCallback callback) override;
  void DeleteItem(const std::string& id, OperationCallback callback) override;
  void DeleteItemByTabId(int64_t tab_id, OperationCallback callback) override;
  void Clear(base::OnceClosure callback) override;
  void GetAllItems(GetAllItemsCallback callback) const override;

 private:
  void NotifyAutoTodosChanged();

  base::ObserverList<Observer> observers_;

  base::LRUCache<std::string, AutoTodoEntry> entries_;
  int64_t next_item_id_ = 1;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_AUTO_TODOS_IN_MEMORY_AUTO_TODOS_STORE_H_
