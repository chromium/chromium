// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_controller_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-forward.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace crosapi {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pointee;

class TestMojomSearchController : public mojom::SearchController {
 public:
  mojo::PendingRemote<mojom::SearchController> BindToRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void ProduceResults(
      mojom::SearchStatus status,
      std::optional<std::vector<mojom::SearchResultPtr>> results) {
    publisher_->OnSearchResultsReceived(status, std::move(results));
  }

  const std::u16string& last_query() { return last_query_; }

 private:
  void Search(const std::u16string& query, SearchCallback callback) override {
    last_query_ = query;

    publisher_.reset();
    std::move(callback).Run(publisher_.BindNewEndpointAndPassReceiver());
  }

  mojo::Receiver<mojom::SearchController> receiver_{this};
  mojo::AssociatedRemote<mojom::SearchResultsPublisher> publisher_;
  std::u16string last_query_;
};

using SearchResultsTestFuture =
    ::base::test::TestFuture<std::vector<mojom::SearchResultPtr>>;

using SearchControllerAshTest = ::testing::Test;

TEST_F(SearchControllerAshTest, CallbackNotCalledIfNotConnected) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;

  std::unique_ptr<SearchControllerAsh> controller;
  {
    TestMojomSearchController mojom_controller;
    controller =
        std::make_unique<SearchControllerAsh>(mojom_controller.BindToRemote());
    // Run until idle to ensure that the controller binds the remote...
    environment.RunUntilIdle();
    // ...then destroy the receiver to disconnect it...
  }
  // ...and ensure that the controller receives the disconnection.
  environment.RunUntilIdle();
  controller->Search(u"cat", future.GetRepeatingCallback());
  environment.RunUntilIdle();

  EXPECT_FALSE(future.IsReady());
}

TEST_F(SearchControllerAshTest, CallbackNotCalledIfBackendUnavailable) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  environment.RunUntilIdle();
  controller.Search(u"cat", future.GetRepeatingCallback());
  environment.RunUntilIdle();
  mojom_controller.ProduceResults(mojom::SearchStatus::kBackendUnavailable,
                                  std::nullopt);
  environment.RunUntilIdle();

  EXPECT_FALSE(future.IsReady());
}

TEST_F(SearchControllerAshTest, CallbackNotCalledIfCancelled) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  environment.RunUntilIdle();
  controller.Search(u"cat", future.GetRepeatingCallback());
  environment.RunUntilIdle();
  mojom_controller.ProduceResults(mojom::SearchStatus::kBackendUnavailable,
                                  std::nullopt);
  environment.RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(SearchControllerAshTest, CallbackCalledWithEmptyResults) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  environment.RunUntilIdle();
  controller.Search(u"cat", future.GetRepeatingCallback());
  environment.RunUntilIdle();
  mojom_controller.ProduceResults(mojom::SearchStatus::kDone,
                                  std::vector<mojom::SearchResultPtr>());

  std::vector<mojom::SearchResultPtr> returned_results = future.Take();
  EXPECT_THAT(returned_results, IsEmpty());
}

TEST_F(SearchControllerAshTest,
       CallbackCalledWithMultipleResultsSimultaneously) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  environment.RunUntilIdle();
  controller.Search(u"cat", future.GetRepeatingCallback());
  environment.RunUntilIdle();
  std::vector<mojom::SearchResultPtr> results;
  {
    mojom::SearchResultPtr result = mojom::SearchResult::New();
    result->destination_url = GURL("https://www.google.com/search?q=cat");
    results.push_back(std::move(result));
  }
  {
    mojom::SearchResultPtr result = mojom::SearchResult::New();
    result->destination_url =
        GURL("https://www.google.com/search?q=catalan+numbers");
    results.push_back(std::move(result));
  }
  mojom_controller.ProduceResults(mojom::SearchStatus::kDone,
                                  std::move(results));

  std::vector<mojom::SearchResultPtr> returned_results = future.Take();
  EXPECT_THAT(
      returned_results,
      ElementsAre(
          Pointee(Field("destination_url",
                        &mojom::SearchResult::destination_url,
                        Optional(GURL("https://www.google.com/search?q=cat")))),
          Pointee(Field(
              "destination_url", &mojom::SearchResult::destination_url,
              Optional(
                  GURL("https://www.google.com/search?q=catalan+numbers"))))));
}

TEST_F(SearchControllerAshTest, CallbackCalledWithMultipleResultsSeparately) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  environment.RunUntilIdle();
  controller.Search(u"cat", future.GetRepeatingCallback());
  environment.RunUntilIdle();

  {
    std::vector<mojom::SearchResultPtr> results;
    mojom::SearchResultPtr result = mojom::SearchResult::New();
    result->destination_url = GURL("https://www.google.com/search?q=cat");
    results.push_back(std::move(result));
    mojom_controller.ProduceResults(mojom::SearchStatus::kDone,
                                    std::move(results));

    std::vector<mojom::SearchResultPtr> returned_results = future.Take();
    EXPECT_THAT(returned_results,
                ElementsAre(Pointee(Field(
                    "destination_url", &mojom::SearchResult::destination_url,
                    Optional(GURL("https://www.google.com/search?q=cat"))))));
  }
  {
    std::vector<mojom::SearchResultPtr> results;
    mojom::SearchResultPtr result = mojom::SearchResult::New();
    result->destination_url =
        GURL("https://www.google.com/search?q=catalan+numbers");
    results.push_back(std::move(result));
    mojom_controller.ProduceResults(mojom::SearchStatus::kDone,
                                    std::move(results));

    std::vector<mojom::SearchResultPtr> returned_results = future.Take();
    EXPECT_THAT(returned_results,
                ElementsAre(Pointee(Field(
                    "destination_url", &mojom::SearchResult::destination_url,
                    Optional(GURL(
                        "https://www.google.com/search?q=catalan+numbers"))))));
  }
}

TEST_F(SearchControllerAshTest, CallbackIsNotCalledWithInProgressResults) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  environment.RunUntilIdle();
  controller.Search(u"cat", future.GetRepeatingCallback());
  environment.RunUntilIdle();

  {
    std::vector<mojom::SearchResultPtr> results;
    mojom::SearchResultPtr result = mojom::SearchResult::New();
    result->destination_url = GURL("https://www.google.com/search?q=cat");
    results.push_back(std::move(result));
    mojom_controller.ProduceResults(mojom::SearchStatus::kInProgress,
                                    std::move(results));
    environment.RunUntilIdle();

    EXPECT_FALSE(future.IsReady());
  }
  {
    std::vector<mojom::SearchResultPtr> results;
    mojom::SearchResultPtr result = mojom::SearchResult::New();
    result->destination_url =
        GURL("https://www.google.com/search?q=catalan+numbers");
    results.push_back(std::move(result));
    mojom_controller.ProduceResults(mojom::SearchStatus::kDone,
                                    std::move(results));

    std::vector<mojom::SearchResultPtr> returned_results = future.Take();
    EXPECT_THAT(returned_results,
                ElementsAre(Pointee(Field(
                    "destination_url", &mojom::SearchResult::destination_url,
                    Optional(GURL(
                        "https://www.google.com/search?q=catalan+numbers"))))));
  }
}

}  // namespace
}  // namespace crosapi
