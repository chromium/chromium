// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/token_results_merger.h"

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace kcer::internal {

// "Move-only" is not required, but should be sufficient. Contains an `id` for
// comparison.
struct MoveOnlyType {
  // A convenience method for creating a vector of objects. The objects will
  // have ids 0, ..., `size`-1 .
  static std::vector<MoveOnlyType> CreateVector(size_t size) {
    return CreateVector(size, /*start_id=*/0);
  }
  // A convenience method for creating a vector of objects. The objects will
  // have ids `start_id`, ..., `start_id`+`size`-`1` .
  static std::vector<MoveOnlyType> CreateVector(size_t size, size_t start_id) {
    std::vector<MoveOnlyType> result;
    for (size_t i = 0; i < size; ++i) {
      result.emplace_back(start_id + i);
    }
    return result;
  }

  explicit MoveOnlyType(size_t object_id) : id(object_id) {}
  ~MoveOnlyType() = default;
  MoveOnlyType(MoveOnlyType&&) = default;
  MoveOnlyType& operator=(MoveOnlyType&&) = default;
  MoveOnlyType(const MoveOnlyType&) = delete;
  MoveOnlyType& operator=(const MoveOnlyType&) = delete;

  bool operator==(const MoveOnlyType& other) const { return (id == other.id); }

  size_t id;
};

namespace {

using Merger = TokenResultsMerger<MoveOnlyType>;

class KcerTokenResultsMergerTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Test that TokenResultsMerger can collect one success result which is returned
// synchronously.
TEST_F(KcerTokenResultsMergerTest, OneSuccessResultSync) {
  base::test::TestFuture<std::vector<MoveOnlyType>,
                         base::flat_map<Token, Error>>
      result_waiter;

  scoped_refptr<Merger> merger = Merger::Create(
      /*results_to_receive=*/1, result_waiter.GetCallback());

  auto merger_callback = merger->GetCallback(Token::kDevice);

  merger.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  std::move(merger_callback).Run(MoveOnlyType::CreateVector(3));

  EXPECT_TRUE(result_waiter.IsReady());
  EXPECT_EQ(result_waiter.Get<0>(), MoveOnlyType::CreateVector(3));
  EXPECT_TRUE(result_waiter.Get<1>().empty());
}

// Test that TokenResultsMerger can collect one success result which is returned
// asynchronously.
TEST_F(KcerTokenResultsMergerTest, OneSuccessResultAsync) {
  base::test::TestFuture<std::vector<MoveOnlyType>,
                         base::flat_map<Token, Error>>
      result_waiter;

  scoped_refptr<Merger> merger = Merger::Create(
      /*results_to_receive=*/1, result_waiter.GetCallback());

  base::OneShotTimer fake_token;  // Runs a callback after a delay.
  fake_token.Start(FROM_HERE, base::Seconds(10),
                   base::BindOnce(merger->GetCallback(Token::kDevice),
                                  MoveOnlyType::CreateVector(3)));

  merger.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(result_waiter.IsReady());
  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_TRUE(result_waiter.IsReady());
  EXPECT_EQ(result_waiter.Get<0>(), MoveOnlyType::CreateVector(3));
  EXPECT_TRUE(result_waiter.Get<1>().empty());
}

// Test that TokenResultsMerger can collect one fail result.
TEST_F(KcerTokenResultsMergerTest, OneFailureResult) {
  base::test::TestFuture<std::vector<MoveOnlyType>,
                         base::flat_map<Token, Error>>
      result_waiter;

  scoped_refptr<Merger> merger = Merger::Create(
      /*results_to_receive=*/1, result_waiter.GetCallback());

  base::OneShotTimer fake_token;  // Runs a callback after a delay.
  fake_token.Start(FROM_HERE, base::Seconds(10),
                   base::BindOnce(merger->GetCallback(Token::kDevice),
                                  base::unexpected(Error::kUnknownError)));

  merger.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(11));

  EXPECT_TRUE(result_waiter.IsReady());
  EXPECT_TRUE(result_waiter.Get<0>().empty());
  const base::flat_map<Token, Error>& error_map = result_waiter.Get<1>();
  ASSERT_TRUE(base::Contains(error_map, Token::kDevice));
  EXPECT_EQ(error_map.at(Token::kDevice), Error::kUnknownError);
}

// Test that TokenResultsMerger can collect two success results.
TEST_F(KcerTokenResultsMergerTest, TwoSuccessResults) {
  base::test::TestFuture<std::vector<MoveOnlyType>,
                         base::flat_map<Token, Error>>
      result_waiter;

  scoped_refptr<Merger> merger = Merger::Create(
      /*results_to_receive=*/2, result_waiter.GetCallback());

  base::OneShotTimer fake_token_1;  // Runs a callback after a delay.
  fake_token_1.Start(FROM_HERE, base::Seconds(3),
                     base::BindOnce(merger->GetCallback(Token::kUser),
                                    MoveOnlyType::CreateVector(3, 0)));

  base::OneShotTimer fake_token_2;  // Runs a callback after a delay.
  fake_token_2.Start(FROM_HERE, base::Seconds(5),
                     base::BindOnce(merger->GetCallback(Token::kDevice),
                                    MoveOnlyType::CreateVector(4, 3)));

  merger.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_TRUE(result_waiter.IsReady());
  EXPECT_EQ(result_waiter.Get<0>(), MoveOnlyType::CreateVector(7));
  EXPECT_TRUE(result_waiter.Get<1>().empty());
}

// Test that TokenResultsMerger can collect two fail results.
TEST_F(KcerTokenResultsMergerTest, TwoFailureResults) {
  base::test::TestFuture<std::vector<MoveOnlyType>,
                         base::flat_map<Token, Error>>
      result_waiter;

  scoped_refptr<Merger> merger = Merger::Create(
      /*results_to_receive=*/2, result_waiter.GetCallback());

  base::OneShotTimer fake_token_1;  // Runs a callback after a delay.
  fake_token_1.Start(
      FROM_HERE, base::Seconds(5),
      base::BindOnce(merger->GetCallback(Token::kUser),
                     base::unexpected(Error::kTokenIsNotAvailable)));

  base::OneShotTimer fake_token_2;  // Runs a callback after a delay.
  fake_token_2.Start(FROM_HERE, base::Seconds(5),
                     base::BindOnce(merger->GetCallback(Token::kDevice),
                                    base::unexpected(Error::kUnknownError)));

  merger.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_TRUE(result_waiter.IsReady());
  EXPECT_TRUE(result_waiter.Get<0>().empty());
  const base::flat_map<Token, Error>& error_map = result_waiter.Get<1>();
  ASSERT_TRUE(base::Contains(error_map, Token::kDevice));
  ASSERT_TRUE(base::Contains(error_map, Token::kUser));
  EXPECT_EQ(error_map.at(Token::kDevice), Error::kUnknownError);
  EXPECT_EQ(error_map.at(Token::kUser), Error::kTokenIsNotAvailable);
}

// Test that TokenResultsMerger can collect one fail with one success results.
TEST_F(KcerTokenResultsMergerTest, OneFailOneSuccessResults) {
  base::test::TestFuture<std::vector<MoveOnlyType>,
                         base::flat_map<Token, Error>>
      result_waiter;

  scoped_refptr<Merger> merger = Merger::Create(
      /*results_to_receive=*/2, result_waiter.GetCallback());

  base::OneShotTimer fake_token_1;  // Runs a callback after a delay.
  fake_token_1.Start(FROM_HERE, base::Seconds(3),
                     base::BindOnce(merger->GetCallback(Token::kUser),
                                    MoveOnlyType::CreateVector(3)));

  base::OneShotTimer fake_token_2;  // Runs a callback after a delay.
  fake_token_2.Start(FROM_HERE, base::Seconds(5),
                     base::BindOnce(merger->GetCallback(Token::kDevice),
                                    base::unexpected(Error::kUnknownError)));

  merger.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_TRUE(result_waiter.IsReady());
  EXPECT_EQ(result_waiter.Get<0>(), MoveOnlyType::CreateVector(3));
  const base::flat_map<Token, Error>& error_map = result_waiter.Get<1>();
  ASSERT_TRUE(base::Contains(error_map, Token::kDevice));
  EXPECT_EQ(error_map.at(Token::kDevice), Error::kUnknownError);
}

}  // namespace
}  // namespace kcer::internal
