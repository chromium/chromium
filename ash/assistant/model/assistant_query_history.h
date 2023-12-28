// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_HISTORY_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_HISTORY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ref.h"

namespace ash {

// Caches user query history.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantQueryHistory {
 public:
  class Iterator {
   public:
    explicit Iterator(const base::circular_deque<std::string>& queries);

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    ~Iterator();

    // Fetches the next query. If current is already the last query, or there is
    // no query in history, returns nullopt.
    std::optional<std::string> Next();

    // Fetches the previous query. If current is already the first query, return
    // the first query. If there is no query in history, returns nullopt.
    std::optional<std::string> Prev();

    // Resets to the last query. It also makes current iterator valid again if
    // new queries are added to the underlying AssistantQueryHistory.
    void ResetToLast();

   private:
    const raw_ref<const base::circular_deque<std::string>> queries_;
    size_t cur_pos_;
  };

  AssistantQueryHistory(int capacity = 100);

  AssistantQueryHistory(const AssistantQueryHistory&) = delete;
  AssistantQueryHistory& operator=(const AssistantQueryHistory&) = delete;

  ~AssistantQueryHistory();

  // Gets the iterator of query history.
  std::unique_ptr<Iterator> GetIterator() const;

  // Adds a query to history. If it is empty, ignore it.
  void Add(const std::string& query);

 private:
  const int capacity_;
  base::circular_deque<std::string> queries_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_HISTORY_H_
