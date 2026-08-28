// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/auto_todos/in_memory_auto_todos_store.h"

#include <utility>
#include <variant>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/features.h"

namespace context_hub {

namespace {
constexpr size_t kMaxEntries = 150;

bool IsExpired(const AutoTodoEntry& entry,
               base::Time now,
               base::TimeDelta ttl) {
  if (entry.last_modified_timestamp.is_null()) {
    return false;
  }
  return (now - entry.last_modified_timestamp) > ttl;
}
}  // namespace

InMemoryAutoTodosStore::InMemoryAutoTodosStore()
    : entries_(kMaxEntries), ttl_(features::kAutoTodosCacheTTL.Get()) {}

InMemoryAutoTodosStore::~InMemoryAutoTodosStore() = default;

void InMemoryAutoTodosStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InMemoryAutoTodosStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool InMemoryAutoTodosStore::DeleteExpiredEntriesInternal() {
  base::Time now = base::Time::Now();
  bool deleted = false;
  auto it = entries_.begin();
  while (it != entries_.end()) {
    if (IsExpired(it->second, now, ttl_)) {
      it = entries_.Erase(it);
      deleted = true;
      continue;
    }
    ++it;
  }
  return deleted;
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
  DeleteExpiredEntriesInternal();
  if (item.id.empty()) {
    item.id = base::StrCat({"todo_", base::NumberToString(next_item_id_++)});
  }
  item.last_modified_timestamp = base::Time::Now();
  std::string id = item.id;
  entries_.Put(id, std::move(item));
  NotifyAutoTodosChanged();
  if (callback) {
    std::move(callback).Run(true);
  }
}

void InMemoryAutoTodosStore::AddAllTodos(base::span<const AutoTodoEntry> items,
                                         OperationCallback callback) {
  DeleteExpiredEntriesInternal();
  base::Time now = base::Time::Now();
  for (AutoTodoEntry item : items) {
    if (item.id.empty()) {
      item.id = base::StrCat({"todo_", base::NumberToString(next_item_id_++)});
    }
    item.last_modified_timestamp = now;
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
  DeleteExpiredEntriesInternal();
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
  DeleteExpiredEntriesInternal();
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

void InMemoryAutoTodosStore::ClearFirstPartyTodos(OperationCallback callback) {
  DeleteExpiredEntriesInternal();
  bool deleted = false;
  auto it = entries_.begin();
  while (it != entries_.end()) {
    if (it->second.is_first_party()) {
      it = entries_.Erase(it);
      deleted = true;
      continue;
    }
    ++it;
  }
  if (deleted) {
    NotifyAutoTodosChanged();
  }
  std::move(callback).Run(true);
}

void InMemoryAutoTodosStore::ClearThirdPartyTodos(OperationCallback callback) {
  DeleteExpiredEntriesInternal();
  bool deleted = false;
  auto it = entries_.begin();
  while (it != entries_.end()) {
    if (it->second.is_third_party()) {
      it = entries_.Erase(it);
      deleted = true;
      continue;
    }
    ++it;
  }
  if (deleted) {
    NotifyAutoTodosChanged();
  }
  std::move(callback).Run(true);
}

void InMemoryAutoTodosStore::DeleteExpiredEntries(OperationCallback callback) {
  bool deleted = DeleteExpiredEntriesInternal();
  if (deleted) {
    NotifyAutoTodosChanged();
  }
  if (callback) {
    std::move(callback).Run(true);
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
