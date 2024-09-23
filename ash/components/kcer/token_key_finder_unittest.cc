// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/token_key_finder.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace kcer::internal {
namespace {

class KcerTokenKeyFinderTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Test that TokenKeyFinder can collect one "key found" result which is returned
// synchronously.
TEST_F(KcerTokenKeyFinderTest, OneKeyFoundResultSync) {
  base::test::TestFuture<base::expected<std::optional<Token>, Error>>
      result_waiter;

  auto finder = TokenKeyFinder::Create(
      /*results_to_receive=*/1, result_waiter.GetCallback());

  auto finder_callback = finder->GetCallback(Token::kDevice);

  finder.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  std::move(finder_callback).Run(/*key_exists=*/true);

  EXPECT_TRUE(result_waiter.IsReady());
  ASSERT_TRUE(result_waiter.Get().has_value());
  EXPECT_EQ(result_waiter.Get().value(), Token::kDevice);
}

// Test that TokenKeyFinder can collect one "key found" result which is returned
// asynchronously.
TEST_F(KcerTokenKeyFinderTest, OneKeyFoundResultAsync) {
  base::test::TestFuture<base::expected<std::optional<Token>, Error>>
      result_waiter;

  auto finder = TokenKeyFinder::Create(
      /*results_to_receive=*/1, result_waiter.GetCallback());

  base::OneShotTimer fake_token;  // Runs a callback after a delay.
  fake_token.Start(FROM_HERE, base::Seconds(10),
                   base::BindOnce(finder->GetCallback(Token::kDevice),
                                  /*key_exists=*/true));

  finder.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(result_waiter.IsReady());
  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_TRUE(result_waiter.IsReady());
  ASSERT_TRUE(result_waiter.Get().has_value());
  EXPECT_EQ(result_waiter.Get().value(), Token::kDevice);
}

// Test that TokenKeyFinder can collect one "key not found" result.
TEST_F(KcerTokenKeyFinderTest, OneKeyNotFoundResult) {
  base::test::TestFuture<base::expected<std::optional<Token>, Error>>
      result_waiter;

  auto finder = TokenKeyFinder::Create(
      /*results_to_receive=*/1, result_waiter.GetCallback());

  base::OneShotTimer fake_token;  // Runs a callback after a delay.
  fake_token.Start(FROM_HERE, base::Seconds(10),
                   base::BindOnce(finder->GetCallback(Token::kDevice),
                                  /*key_exists=*/false));

  finder.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(11));

  EXPECT_TRUE(result_waiter.IsReady());
  ASSERT_TRUE(result_waiter.Get().has_value());
  EXPECT_FALSE(result_waiter.Get().value().has_value());
}

// Test that TokenKeyFinder can collect one failure result.
TEST_F(KcerTokenKeyFinderTest, OneFailureResult) {
  base::test::TestFuture<base::expected<std::optional<Token>, Error>>
      result_waiter;

  auto finder = TokenKeyFinder::Create(
      /*results_to_receive=*/1, result_waiter.GetCallback());

  base::OneShotTimer fake_token;  // Runs a callback after a delay.
  fake_token.Start(FROM_HERE, base::Seconds(10),
                   base::BindOnce(finder->GetCallback(Token::kUser),
                                  /*key_exists=*/base::unexpected(
                                      Error::kTokenInitializationFailed)));

  finder.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(11));

  EXPECT_TRUE(result_waiter.IsReady());
  ASSERT_FALSE(result_waiter.Get().has_value());
  EXPECT_EQ(result_waiter.Get().error(), Error::kTokenInitializationFailed);
}

// Test that TokenKeyFinder indicates when a key is found when it's found on
// both tokens (not supposed to happen, but still a theoretically possible edge
// case).
TEST_F(KcerTokenKeyFinderTest, TwoKeysFound) {
  base::test::TestFuture<base::expected<std::optional<Token>, Error>>
      result_waiter;

  auto finder = TokenKeyFinder::Create(
      /*results_to_receive=*/2, result_waiter.GetCallback());

  base::OneShotTimer fake_token_1;  // Runs a callback after a delay.
  fake_token_1.Start(FROM_HERE, base::Seconds(3),
                     base::BindOnce(finder->GetCallback(Token::kUser),
                                    /*key_exists=*/true));

  base::OneShotTimer fake_token_2;  // Runs a callback after a delay.
  fake_token_2.Start(FROM_HERE, base::Seconds(5),
                     base::BindOnce(finder->GetCallback(Token::kDevice),
                                    /*key_exists=*/true));

  finder.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_TRUE(result_waiter.IsReady());
  ASSERT_TRUE(result_waiter.Get().has_value());
  EXPECT_TRUE(result_waiter.Get().value().has_value());
}

// Test that TokenKeyFinder indicates that the key was not found when it was not
// found on both tokens.
TEST_F(KcerTokenKeyFinderTest, TwoTokensKeyNotFound) {
  base::test::TestFuture<base::expected<std::optional<Token>, Error>>
      result_waiter;

  auto finder = TokenKeyFinder::Create(
      /*results_to_receive=*/2, result_waiter.GetCallback());

  base::OneShotTimer fake_token_1;  // Runs a callback after a delay.
  fake_token_1.Start(FROM_HERE, base::Seconds(3),
                     base::BindOnce(finder->GetCallback(Token::kUser),
                                    /*key_exists=*/false));

  base::OneShotTimer fake_token_2;  // Runs a callback after a delay.
  fake_token_2.Start(FROM_HERE, base::Seconds(5),
                     base::BindOnce(finder->GetCallback(Token::kDevice),
                                    /*key_exists=*/false));

  finder.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_TRUE(result_waiter.IsReady());
  ASSERT_TRUE(result_waiter.Get().has_value());
  EXPECT_FALSE(result_waiter.Get().value().has_value());
}

// Test that TokenKeyFinder indicates that the key was found when it was found
// on one of the tokens.
TEST_F(KcerTokenKeyFinderTest, TwoTokensOneKeyFound) {
  base::test::TestFuture<base::expected<std::optional<Token>, Error>>
      result_waiter;

  auto finder = TokenKeyFinder::Create(
      /*results_to_receive=*/2, result_waiter.GetCallback());

  base::OneShotTimer fake_token_1;  // Runs a callback after a delay.
  fake_token_1.Start(FROM_HERE, base::Seconds(10),
                     base::BindOnce(finder->GetCallback(Token::kUser),
                                    /*key_exists=*/false));

  base::OneShotTimer fake_token_2;  // Runs a callback after a delay.
  fake_token_2.Start(FROM_HERE, base::Seconds(20),
                     base::BindOnce(finder->GetCallback(Token::kDevice),
                                    /*key_exists=*/true));

  finder.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(15));
  EXPECT_FALSE(result_waiter.IsReady());
  task_environment_.FastForwardBy(base::Seconds(7));

  EXPECT_TRUE(result_waiter.IsReady());
  ASSERT_TRUE(result_waiter.Get().has_value());
  EXPECT_EQ(result_waiter.Get().value(), Token::kDevice);
}

// Test that TokenKeyFinder returns an error when the key is not found on one
// token and another one returned an error.
TEST_F(KcerTokenKeyFinderTest, OneErrorOneNotFound) {
  base::test::TestFuture<base::expected<std::optional<Token>, Error>>
      result_waiter;

  auto finder = TokenKeyFinder::Create(
      /*results_to_receive=*/2, result_waiter.GetCallback());

  base::OneShotTimer fake_token_1;  // Runs a callback after a delay.
  fake_token_1.Start(FROM_HERE, base::Seconds(3),
                     base::BindOnce(finder->GetCallback(Token::kUser),
                                    /*key_exists=*/base::unexpected(
                                        Error::kTokenInitializationFailed)));

  base::OneShotTimer fake_token_2;  // Runs a callback after a delay.
  fake_token_2.Start(FROM_HERE, base::Seconds(4),
                     base::BindOnce(finder->GetCallback(Token::kDevice),
                                    /*key_exists=*/false));

  finder.reset();  // It should be ok to delete the original ref-pointer now.

  EXPECT_FALSE(result_waiter.IsReady());

  task_environment_.FastForwardBy(base::Seconds(7));

  EXPECT_TRUE(result_waiter.IsReady());
  ASSERT_FALSE(result_waiter.Get().has_value());
  EXPECT_EQ(result_waiter.Get().error(), Error::kTokenInitializationFailed);
}

}  // namespace
}  // namespace kcer::internal
