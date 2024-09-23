// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_controller_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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

// TODO: b/326147929 - Share this code with `crosapi::SearchControllerAsh` unit
// tests (and possibly `app_list::OmniboxLacrosProvider` unit tests too).
class TestMojomSearchController : public mojom::SearchController {
 public:
  mojo::PendingRemote<mojom::SearchController> BindToRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void RunUntilSearch() {
    base::test::TestFuture<void> future;
    base::AutoReset<base::RepeatingClosure> quit_loop(
        &search_callback_, future.GetRepeatingCallback());
    EXPECT_TRUE(future.Wait());
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

    search_callback_.Run();
  }

  base::RepeatingClosure search_callback_ = base::DoNothing();

  mojo::Receiver<mojom::SearchController> receiver_{this};
  mojo::AssociatedRemote<mojom::SearchResultsPublisher> publisher_;
  std::u16string last_query_;
};

using SearchResultsTestFuture =
    ::base::test::TestFuture<std::vector<mojom::SearchResultPtr>>;
using DisconnectTestFuture =
    ::base::test::TestFuture<::base::WeakPtr<::crosapi::SearchControllerAsh>>;

using SearchControllerAshTest = ::testing::Test;

TEST_F(SearchControllerAshTest, CallbackNotCalledIfNotConnected) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;

  std::unique_ptr<SearchControllerAsh> controller;
  {
    TestMojomSearchController mojom_controller;
    controller =
        std::make_unique<SearchControllerAsh>(mojom_controller.BindToRemote());
  }
  {
    DisconnectTestFuture future1;
    controller->AddDisconnectHandler(future1.GetCallback());
    EXPECT_TRUE(future1.Wait());
  }
  controller->Search(u"cat", future.GetRepeatingCallback());

  EXPECT_FALSE(future.IsReady());
}

TEST_F(SearchControllerAshTest, CallbackNotCalledIfBackendUnavailable) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  controller.Search(u"cat", future.GetRepeatingCallback());
  mojom_controller.RunUntilSearch();
  mojom_controller.ProduceResults(mojom::SearchStatus::kBackendUnavailable,
                                  std::nullopt);
  // Run until `controller.OnSearchResultsReceived()` is called.
  // TODO: b/326147929 - Use a `QuitClosure` for this.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(future.IsReady());
}

TEST_F(SearchControllerAshTest, CallbackNotCalledIfCancelled) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  controller.Search(u"cat", future.GetRepeatingCallback());
  mojom_controller.RunUntilSearch();
  mojom_controller.ProduceResults(mojom::SearchStatus::kBackendUnavailable,
                                  std::nullopt);
  // Run until `controller.OnSearchResultsReceived()` is called.
  // TODO: b/326147929 - Use a `QuitClosure` for this.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(future.IsReady());
}

TEST_F(SearchControllerAshTest, CallbackCalledWithEmptyResults) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchResultsTestFuture future;
  TestMojomSearchController mojom_controller;

  SearchControllerAsh controller(mojom_controller.BindToRemote());
  controller.Search(u"cat", future.GetRepeatingCallback());
  mojom_controller.RunUntilSearch();
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
  controller.Search(u"cat", future.GetRepeatingCallback());
  mojom_controller.RunUntilSearch();
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
  controller.Search(u"cat", future.GetRepeatingCallback());
  mojom_controller.RunUntilSearch();

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
  controller.Search(u"cat", future.GetRepeatingCallback());
  mojom_controller.RunUntilSearch();

  {
    std::vector<mojom::SearchResultPtr> results;
    mojom::SearchResultPtr result = mojom::SearchResult::New();
    result->destination_url = GURL("https://www.google.com/search?q=cat");
    results.push_back(std::move(result));
    mojom_controller.ProduceResults(mojom::SearchStatus::kInProgress,
                                    std::move(results));
    // Run until `controller.OnSearchResultsReceived()` is run.
    // TODO: b/326147929 - Use a `QuitClosure` for this.
    base::RunLoop().RunUntilIdle();

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

TEST_F(SearchControllerAshTest,
       DisconnectHandlerIsCalledOnDisconnectWithValidPointer) {
  base::test::SingleThreadTaskEnvironment environment;
  DisconnectTestFuture future;
  auto mojom_controller = std::make_unique<TestMojomSearchController>();
  SearchControllerAsh controller(mojom_controller->BindToRemote());

  controller.AddDisconnectHandler(future.GetCallback());
  mojom_controller.reset();
  ASSERT_TRUE(future.Wait()) << "Disconnect handler was never called";
  ASSERT_FALSE(controller.IsConnected());

  EXPECT_TRUE(future.IsReady());
  base::WeakPtr<SearchControllerAsh> weak_controller = future.Take();
  EXPECT_TRUE(weak_controller);
  EXPECT_EQ(weak_controller.get(), &controller);
}

TEST_F(SearchControllerAshTest,
       DisconnectHandlersAreCalledOnDisconnectWithValidPointers) {
  base::test::SingleThreadTaskEnvironment environment;
  DisconnectTestFuture future_1;
  DisconnectTestFuture future_2;
  auto mojom_controller = std::make_unique<TestMojomSearchController>();
  SearchControllerAsh controller(mojom_controller->BindToRemote());

  controller.AddDisconnectHandler(future_1.GetCallback());
  controller.AddDisconnectHandler(future_2.GetCallback());
  mojom_controller.reset();
  ASSERT_TRUE(future_2.Wait()) << "Disconnect handler was never called";
  ASSERT_FALSE(controller.IsConnected());

  base::WeakPtr<SearchControllerAsh> weak_controller_1 = future_1.Take();
  EXPECT_TRUE(weak_controller_1);
  EXPECT_EQ(weak_controller_1.get(), &controller);
  base::WeakPtr<SearchControllerAsh> weak_controller_2 = future_2.Take();
  EXPECT_TRUE(weak_controller_2);
  EXPECT_EQ(weak_controller_2.get(), &controller);
}

TEST_F(SearchControllerAshTest, DisconnectHandlersAreCalledInAdditionOrder) {
  base::test::SingleThreadTaskEnvironment environment;
  DisconnectTestFuture future_1;
  DisconnectTestFuture future_2;
  auto mojom_controller = std::make_unique<TestMojomSearchController>();
  SearchControllerAsh controller(mojom_controller->BindToRemote());

  controller.AddDisconnectHandler(
      future_1.GetCallback().Then(base::BindLambdaForTesting([&future_2]() {
        EXPECT_FALSE(future_2.IsReady())
            << "Second future called before first future";
      })));
  controller.AddDisconnectHandler(future_2.GetCallback());
  mojom_controller.reset();
  ASSERT_TRUE(future_2.Wait()) << "Disconnect handler was never called";
  ASSERT_FALSE(controller.IsConnected());

  // This also guarantees that the "first future called before second future"
  // `EXPECT` above is run.
  EXPECT_TRUE(future_1.IsReady()) << "First future not called";
  EXPECT_TRUE(future_2.IsReady()) << "Second future not called";
}

TEST_F(SearchControllerAshTest,
       DisconnectHandlerIsImmediatelyCalledIfAlreadyDisconnected) {
  base::test::SingleThreadTaskEnvironment environment;
  DisconnectTestFuture future;
  auto mojom_controller = std::make_unique<TestMojomSearchController>();
  SearchControllerAsh controller(mojom_controller->BindToRemote());
  mojom_controller.reset();
  {
    DisconnectTestFuture future1;
    controller.AddDisconnectHandler(future1.GetCallback());
    EXPECT_TRUE(future1.Wait());
  }
  ASSERT_FALSE(controller.IsConnected());

  controller.AddDisconnectHandler(future.GetCallback());

  EXPECT_TRUE(future.IsReady());
}

TEST_F(SearchControllerAshTest,
       DisconnectHandlerIsNotCalledIfNeverDisconnected) {
  base::test::SingleThreadTaskEnvironment environment;
  DisconnectTestFuture future;
  TestMojomSearchController mojom_controller;
  auto controller =
      std::make_unique<SearchControllerAsh>(mojom_controller.BindToRemote());

  controller->AddDisconnectHandler(future.GetCallback());
  controller.reset();

  EXPECT_FALSE(future.IsReady());
}

TEST_F(SearchControllerAshTest,
       DisconnectHandlerIsCalledWithNullptrIfDestructed) {
  base::test::SingleThreadTaskEnvironment environment;
  DisconnectTestFuture future;
  auto mojom_controller = std::make_unique<TestMojomSearchController>();
  auto controller =
      std::make_unique<SearchControllerAsh>(mojom_controller->BindToRemote());

  controller->AddDisconnectHandler(base::BindLambdaForTesting(
      [&controller](base::WeakPtr<SearchControllerAsh>) {
        controller.reset();
      }));
  controller->AddDisconnectHandler(future.GetCallback());
  mojom_controller.reset();

  base::WeakPtr<SearchControllerAsh> weak_controller = future.Take();
  EXPECT_FALSE(weak_controller);
}

}  // namespace
}  // namespace crosapi
