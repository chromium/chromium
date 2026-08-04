// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/auto_todos/in_memory_auto_todos_store.h"

#include <utility>
#include <variant>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace context_hub {

namespace {
constexpr size_t kMaxEntries = 150;
}  // namespace

InMemoryAutoTodosStore::InMemoryAutoTodosStore() : entries_(kMaxEntries) {}
InMemoryAutoTodosStore::~InMemoryAutoTodosStore() = default;

void InMemoryAutoTodosStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InMemoryAutoTodosStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InMemoryAutoTodosStore::NotifyAutoTodosChanged() {
  std::vector<AutoTodoEntry> result;
  result.reserve(entries_.size());
  for (const auto& [id, entry] : entries_) {
    result.push_back(entry);
  }
  observers_.Notify(&Observer::OnAutoTodosChanged, result);
}

void InMemoryAutoTodosStore::AddOrUpdateItem(AutoTodoEntry item,
                                             OperationCallback callback) {
  if (item.id.empty()) {
    item.id = base::StrCat({"item_", base::NumberToString(next_item_id_++)});
  }
  std::string id = item.id;
  entries_.Put(id, std::move(item));
  NotifyAutoTodosChanged();
  if (callback) {
    std::move(callback).Run(true);
  }
}

void InMemoryAutoTodosStore::AddAllTodos(base::span<const AutoTodoEntry> items,
                                         OperationCallback callback) {
  for (AutoTodoEntry item : items) {
    if (item.id.empty()) {
      item.id = base::StrCat({"item_", base::NumberToString(next_item_id_++)});
    }
    std::string id = item.id;
    entries_.Put(id, std::move(item));
  }
  NotifyAutoTodosChanged();
  if (callback) {
    std::move(callback).Run(true);
  }
}

void InMemoryAutoTodosStore::DeleteItem(const std::string& id,
                                        OperationCallback callback) {
  auto it = entries_.Peek(id);
  if (it == entries_.end()) {
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }
  entries_.Erase(it);
  NotifyAutoTodosChanged();
  if (callback) {
    std::move(callback).Run(true);
  }
}

void InMemoryAutoTodosStore::DeleteItemByTabId(int64_t tab_id,
                                               OperationCallback callback) {
  bool deleted = false;
  auto it = entries_.begin();
  while (it != entries_.end()) {
    if (const auto* third_party =
            std::get_if<ThirdPartyData>(&it->second.data)) {
      if (third_party->tab_id == tab_id) {
        it = entries_.Erase(it);
        deleted = true;
        continue;
      }
    }
    ++it;
  }
  if (deleted) {
    NotifyAutoTodosChanged();
  }
  if (callback) {
    std::move(callback).Run(deleted);
  }
}

void InMemoryAutoTodosStore::Clear(base::OnceClosure callback) {
  entries_.Clear();
  next_item_id_ = 1;
  NotifyAutoTodosChanged();
  if (callback) {
    std::move(callback).Run();
  }
}

void InMemoryAutoTodosStore::GetAllItems(GetAllItemsCallback callback) const {
  std::vector<AutoTodoEntry> result;
  result.reserve(entries_.size());
  for (const auto& [id, entry] : entries_) {
    result.push_back(entry);
  }
  if (callback) {
    std::move(callback).Run(std::move(result));
  }
}

}  // namespace context_hub
