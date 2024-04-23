// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_query_history.h"

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Assert that iterator Prev() or Next() does not crash on an empty history.
TEST(AssistantQueryHistory, Empty) {
  AssistantQueryHistory history(10);
  auto it = history.GetIterator();
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ(std::nullopt, it->Prev());
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ(std::nullopt, it->Prev());
}

TEST(AssistantQueryHistory, Full) {
  int size = 3;
  AssistantQueryHistory history(size);
  // Make more queries than history limit.
  for (int i = 0; i <= size; i++)
    history.Add(base::NumberToString(i));
  auto it = history.GetIterator();
  // Assert history only contains last 3 queries.
  for (int i = size; i > 0; i--)
    EXPECT_EQ(base::NumberToString(i), it->Prev().value());
  EXPECT_EQ(base::NumberToString(1), it->Prev().value());
  // Assert that iterate does not pass first query.
  EXPECT_EQ(base::NumberToString(1), it->Prev().value());

  // Make more queries than history limit again.
  for (int i = 0; i <= size; i++)
    history.Add(base::NumberToString(i + 4));
  it->ResetToLast();
  // Assert that history only contains last 3 queries.
  for (int i = size; i > 0; i--)
    EXPECT_EQ(base::NumberToString(i + 4), it->Prev());
  EXPECT_EQ(base::NumberToString(5), it->Prev().value());
  // Assert that iterate does not pass first query.
  EXPECT_EQ(base::NumberToString(5), it->Prev().value());
}

TEST(AssistantQueryHistory, Add) {
  AssistantQueryHistory history(10);
  auto it = history.GetIterator();
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ(std::nullopt, it->Next());
  history.Add("Query01");
  history.Add("Query02");
  it = history.GetIterator();
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ("Query02", it->Prev().value());
  EXPECT_EQ("Query01", it->Prev().value());
  EXPECT_EQ("Query01", it->Prev().value());
  EXPECT_EQ("Query02", it->Next().value());
  EXPECT_EQ(std::nullopt, it->Next());
  EXPECT_EQ(std::nullopt, it->Next());
}

}  // namespace ash
