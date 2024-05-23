// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/search_controller_factory_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/search_controller_ash.h"
#include "chrome/browser/ui/ash/picker/picker_lacros_omnibox_search_provider.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-forward.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-shared.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace crosapi {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Optional;
using ::testing::Pointee;

// TODO: b/326147929 - Share this code with `crosapi::SearchControllerAsh` unit
// tests.
class TestMojomSearchController : public mojom::SearchController {
 public:
  explicit TestMojomSearchController(bool bookmarks,
                                     bool history,
                                     bool open_tabs)
      : bookmarks_(bookmarks), history_(history), open_tabs_(open_tabs) {}
  ~TestMojomSearchController() override = default;

  void Bind(mojo::PendingReceiver<mojom::SearchController> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void RunUntilSearch() {
    base::test::TestFuture<void> waiter;
    base::AutoReset<base::RepeatingClosure> quit_loop(
        &search_callback_, waiter.GetRepeatingCallback());
    EXPECT_TRUE(waiter.Wait());
  }

  void ProduceResults(
      mojom::SearchStatus status,
      std::optional<std::vector<mojom::SearchResultPtr>> results) {
    publisher_->OnSearchResultsReceived(status, std::move(results));
  }

  const std::u16string& last_query() { return last_query_; }
  bool bookmarks() const { return bookmarks_; }
  bool history() const { return history_; }
  bool open_tabs() const { return open_tabs_; }

 private:
  // mojom::SearchController overrides:
  void Search(const std::u16string& query, SearchCallback callback) override {
    last_query_ = query;

    publisher_.reset();
    std::move(callback).Run(publisher_.BindNewEndpointAndPassReceiver());

    search_callback_.Run();
  }

  base::RepeatingClosure search_callback_ = base::DoNothing();

  mojo::Receiver<mojom::SearchController> receiver_{this};
  mojo::AssociatedRemote<mojom::SearchResultsPublisher> publisher_;
  bool bookmarks_;
  bool history_;
  bool open_tabs_;
  std::u16string last_query_;
};

class TestMojomSearchControllerFactory : public mojom::SearchControllerFactory {
 public:
  TestMojomSearchControllerFactory() = default;
  ~TestMojomSearchControllerFactory() override = default;

  mojo::PendingRemote<mojom::SearchControllerFactory> BindToRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void RunUntilCreateSearchController() {
    base::test::TestFuture<void> waiter;
    base::AutoReset<base::RepeatingClosure> quit_loop(
        &create_search_controller_callback_, waiter.GetRepeatingCallback());
    EXPECT_TRUE(waiter.Wait());
  }

  std::unique_ptr<TestMojomSearchController> TakeLastTestController() {
    return std::move(last_test_controller_);
  }

  // mojom::SearchControllerFactory overrides:
  void CreateSearchControllerPicker(
      mojo::PendingReceiver<mojom::SearchController> controller,
      bool bookmark,
      bool history,
      bool open_tab) override {
    CHECK(!last_test_controller_);
    last_test_controller_ = std::make_unique<TestMojomSearchController>(
        bookmark, history, open_tab);
    last_test_controller_->Bind(std::move(controller));

    create_search_controller_callback_.Run();
  }

 private:
  base::RepeatingClosure create_search_controller_callback_ = base::DoNothing();

  std::unique_ptr<TestMojomSearchController> last_test_controller_;
  mojo::Receiver<mojom::SearchControllerFactory> receiver_{this};
};

class TestFactoryObserver : public SearchControllerFactoryAsh::Observer {
 public:
  explicit TestFactoryObserver(SearchControllerFactoryAsh* factory) {
    obs_.Observe(factory);
  }
  ~TestFactoryObserver() override = default;

  void OnSearchControllerFactoryBound(
      SearchControllerFactoryAsh* factory) override {
    on_bound_.SetValue(factory);
  }

  base::test::TestFuture<SearchControllerFactoryAsh*>& on_bound() {
    return on_bound_;
  }

 private:
  base::test::TestFuture<SearchControllerFactoryAsh*> on_bound_;

  base::ScopedObservation<SearchControllerFactoryAsh,
                          SearchControllerFactoryAsh::Observer>
      obs_{this};
};

using SearchControllerFactoryAshTest = ::testing::Test;

TEST_F(SearchControllerFactoryAshTest,
       CreateSearchControllerPickerReturnsNullptrIfNotBound) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  ASSERT_FALSE(factory.IsBound());

  std::unique_ptr<SearchControllerAsh> controller =
      factory.CreateSearchControllerPicker(false, false, false);

  EXPECT_FALSE(controller);
}

TEST_F(SearchControllerFactoryAshTest,
       CreateSearchControllerPickerReturnsNonNullIfBoundAndConnected) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  ASSERT_TRUE(factory.IsBound());

  std::unique_ptr<SearchControllerAsh> controller =
      factory.CreateSearchControllerPicker(false, false, false);

  EXPECT_TRUE(controller);
}

TEST_F(SearchControllerFactoryAshTest,
       CreateSearchControllerPickerReturnsNonNullIfBoundAndDisconnected) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  {
    TestMojomSearchControllerFactory mojom_factory;
    factory.BindRemote(mojom_factory.BindToRemote());
    ASSERT_TRUE(factory.IsBound());
  }
  // Ensure that the factory receives the disconnection.
  // TODO: b/326147929 - Use a `QuitClosure` for this.
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<SearchControllerAsh> controller =
      factory.CreateSearchControllerPicker(false, false, false);

  EXPECT_TRUE(controller);
}

TEST_F(SearchControllerFactoryAshTest,
       CreateSearchControllerPickerCalledRemote) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());

  std::unique_ptr<SearchControllerAsh> controller =
      factory.CreateSearchControllerPicker(false, false, false);
  mojom_factory.RunUntilCreateSearchController();

  std::unique_ptr<TestMojomSearchController> mojom_controller =
      mojom_factory.TakeLastTestController();
  EXPECT_TRUE(mojom_controller);
}

TEST_F(SearchControllerFactoryAshTest, CreatedPickerControllerReceivesQueries) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  std::unique_ptr<SearchControllerAsh> controller =
      factory.CreateSearchControllerPicker(false, false, false);
  mojom_factory.RunUntilCreateSearchController();
  std::unique_ptr<TestMojomSearchController> mojom_controller =
      mojom_factory.TakeLastTestController();

  controller->Search(u"cat", base::DoNothing());
  mojom_controller->RunUntilSearch();

  EXPECT_EQ(mojom_controller->last_query(), u"cat");
}

TEST_F(SearchControllerFactoryAshTest, CreatedPickerControllerReceivesResults) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  std::unique_ptr<SearchControllerAsh> controller =
      factory.CreateSearchControllerPicker(false, false, false);
  mojom_factory.RunUntilCreateSearchController();
  std::unique_ptr<TestMojomSearchController> mojom_controller =
      mojom_factory.TakeLastTestController();
  base::test::TestFuture<std::vector<mojom::SearchResultPtr>> search_future;

  controller->Search(u"cat", search_future.GetRepeatingCallback());
  mojom_controller->RunUntilSearch();
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
  mojom_controller->ProduceResults(mojom::SearchStatus::kDone,
                                   std::move(results));

  std::vector<mojom::SearchResultPtr> returned_results = search_future.Take();
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

TEST_F(SearchControllerFactoryAshTest,
       CreatedPickerControllerHasCorrectProviderTypes) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());

  std::unique_ptr<SearchControllerAsh> unused_controller =
      factory.CreateSearchControllerPicker(/*bookmarks=*/true,
                                           /*history=*/false,
                                           /*open_tabs=*/true);
  mojom_factory.RunUntilCreateSearchController();

  std::unique_ptr<TestMojomSearchController> mojom_controller =
      mojom_factory.TakeLastTestController();
  ASSERT_TRUE(mojom_controller);
  EXPECT_TRUE(mojom_controller->bookmarks());
  EXPECT_FALSE(mojom_controller->history());
  EXPECT_TRUE(mojom_controller->open_tabs());
}

TEST_F(SearchControllerFactoryAshTest, OnBoundNotCalledIfNotBound) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;

  TestFactoryObserver observer(&factory);

  EXPECT_FALSE(observer.on_bound().IsReady());
}

TEST_F(SearchControllerFactoryAshTest, OnBoundCalledIfAlreadyBound) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());

  TestFactoryObserver observer(&factory);

  EXPECT_TRUE(observer.on_bound().IsReady());
}

TEST_F(SearchControllerFactoryAshTest, OnBoundCalledWhenBoundLater) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestFactoryObserver observer(&factory);
  ASSERT_FALSE(observer.on_bound().IsReady());

  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());

  EXPECT_TRUE(observer.on_bound().IsReady());
}

TEST_F(SearchControllerFactoryAshTest, OnBoundCalledWhenRebound) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestFactoryObserver observer(&factory);
  {
    // Connect a remote factory...
    TestMojomSearchControllerFactory mojom_factory;
    factory.BindRemote(mojom_factory.BindToRemote());
    ASSERT_TRUE(observer.on_bound().IsReady());
    observer.on_bound().Clear();
    // ...then disconnect it.
  }
  // Ensure that the factory receives the disconnection so it can be rebound.
  // TODO: b/326147929 - Use a `QuitClosure` for this.
  base::RunLoop().RunUntilIdle();

  // Rebind another remote factory.
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());

  EXPECT_TRUE(observer.on_bound().IsReady());
}

TEST_F(SearchControllerFactoryAshTest, OnBoundCalledWithFactoryPtr) {
  base::test::SingleThreadTaskEnvironment environment;
  SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());

  TestFactoryObserver observer(&factory);

  SearchControllerFactoryAsh* ptr = observer.on_bound().Take();
  EXPECT_EQ(ptr, &factory);
}

}  // namespace
}  // namespace crosapi
