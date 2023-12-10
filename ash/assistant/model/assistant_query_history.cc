// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_query_history.h"

namespace ash {

AssistantQueryHistory::AssistantQueryHistory(int capacity)
    : capacity_(capacity) {
  queries_.reserve(capacity);
}

AssistantQueryHistory::~AssistantQueryHistory() = default;

std::unique_ptr<AssistantQueryHistory::Iterator>
AssistantQueryHistory::GetIterator() const {
  return std::make_unique<AssistantQueryHistory::Iterator>(queries_);
}

void AssistantQueryHistory::Add(const std::string& query) {
  if (query.empty())
    return;

  if (static_cast<int>(queries_.size()) == capacity_)
    queries_.pop_front();
  queries_.push_back(query);
}

AssistantQueryHistory::Iterator::Iterator(
    const base::circular_deque<std::string>& queries)
    : queries_(queries), cur_pos_(queries_->size()) {}

AssistantQueryHistory::Iterator::~Iterator() = default;

std::optional<std::string> AssistantQueryHistory::Iterator::Next() {
  // queries_.size() is of type unsigned int and queries_.size() -1 will
  // overflow if it is 0.
  if (cur_pos_ + 1 >= queries_->size()) {
    cur_pos_ = queries_->size();
    return std::nullopt;
  }
  cur_pos_++;
  return std::make_optional<std::string>((*queries_)[cur_pos_]);
}

std::optional<std::string> AssistantQueryHistory::Iterator::Prev() {
  if (queries_->size() == 0) {
    return std::nullopt;
  }

  if (cur_pos_ != 0)
    cur_pos_--;

  return std::make_optional<std::string>((*queries_)[cur_pos_]);
}

void AssistantQueryHistory::Iterator::ResetToLast() {
  cur_pos_ = queries_->size();
}

}  // namespace ash
