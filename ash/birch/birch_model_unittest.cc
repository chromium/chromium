// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_model.h"

#include <optional>

#include "ash/birch/birch_item.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class StubBirchClient : public BirchClient {
 public:
  StubBirchClient() = default;
  ~StubBirchClient() override = default;

  // BirchClient:
  void RequestBirchDataFetch() override {}
};

class TestModelConsumer {
 public:
  void OnItemsReady(const std::string& id) {
    items_ready_responses_.push_back(id);
  }

  const std::vector<std::string> items_ready_responses() const {
    return items_ready_responses_;
  }

 private:
  std::vector<std::string> items_ready_responses_;
};

}  // namespace

class BirchModelTest : public AshTestBase {
 public:
  BirchModelTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kBirchWeather}, {});
  }

  void SetUp() override {
    switches::SetIgnoreForestSecretKeyForTest(true);
    AshTestBase::SetUp();
    // Inject no-op, stub weather provider to prevent real implementation from
    // returning empty weather info.
    Shell::Get()->birch_model()->OverrideWeatherProviderForTest(
        std::make_unique<StubBirchClient>());
    Shell::Get()->birch_model()->SetClient(&stub_birch_client_);
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClient(nullptr);
    AshTestBase::TearDown();
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  StubBirchClient stub_birch_client_;
};

class BirchModelWithoutWeatherTest : public AshTestBase {
 public:
  BirchModelWithoutWeatherTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    switches::SetIgnoreForestSecretKeyForTest(true);
    AshTestBase::SetUp();
    Shell::Get()->birch_model()->SetClient(&stub_birch_client_);
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClient(nullptr);
    AshTestBase::TearDown();
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kForestFeature};
  StubBirchClient stub_birch_client_;
};

// Test that requesting data and adding all fresh items to the model will run
// the callback.
TEST_F(BirchModelTest, AddItemNotifiesCallback) {
  BirchModel* model = Shell::Get()->birch_model();
  TestModelConsumer consumer;
  EXPECT_TRUE(model);

  // Setting items in the model does not notify when no request has occurred.
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  // Make a data fetch request and set fresh tab data.
  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"0"));
  model->SetRecentTabItems(std::vector<BirchTabItem>());

  // Consumer is not notified until all data sources have responded.
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), std::nullopt);
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetWeatherItems({});
  model->SetCalendarItems({});

  // Adding file items sets all data as fresh, notifying consumers.
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  // Setting the file suggest items should not trigger items ready again, since
  // no data fetch was requested.
  file_item_list.emplace_back(base::FilePath("test path 1"), std::nullopt);
  file_item_list.emplace_back(base::FilePath("test path 2"), std::nullopt);
  model->SetFileSuggestItems(std::move(file_item_list));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  // Request another data fetch and expect the consumer to be notified once
  // items are set again.
  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"1"));
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetWeatherItems({});
  model->SetCalendarItems({});
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0", "1"));
}

// TODO(https://crbug.com/324963992): Fix `BirchModel*Test.DataFetchTimeout`
// for debug builds.
#if defined(NDEBUG)
#define MAYBE_DataFetchTimeout DataFetchTimeout
#else
#define MAYBE_DataFetchTimeout DISABLED_DataFetchTimeout
#endif

// Test that consumer is notified when waiting a set amount of time after
// requesting birch data.
TEST_F(BirchModelTest, MAYBE_DataFetchTimeout) {
  BirchModel* model = Shell::Get()->birch_model();
  TestModelConsumer consumer;
  EXPECT_TRUE(model);

  // Passing time and setting data before requesting a birch data fetch will
  // not notify consumer.
  task_environment()->FastForwardBy(base::Milliseconds(1000));

  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), std::nullopt);
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  std::vector<BirchWeatherItem> weather_items;
  weather_items.emplace_back(u"desc", u"temp", ui::ImageModel());
  model->SetWeatherItems(std::move(weather_items));
  model->SetCalendarItems({});

  EXPECT_TRUE(model->IsDataFresh());
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"0"));
  EXPECT_FALSE(model->IsDataFresh());
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  // Test that passing a short amount of time and setting some data does not
  // notify that items are ready.
  task_environment()->FastForwardBy(base::Milliseconds(500));

  std::vector<BirchTabItem> tab_item_list;
  tab_item_list.emplace_back(u"tab title", GURL("example.com"),
                             base::Time::Now(), GURL("example.com/favicon_url"),
                             "session_name");
  model->SetRecentTabItems(tab_item_list);
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  // Test that passing enough time notifies that items are ready.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  std::vector<std::unique_ptr<BirchItem>> all_items = model->GetAllItems();
  EXPECT_EQ(all_items.size(), 3u);
  EXPECT_STREQ(all_items[0]->GetItemType(), BirchTabItem::kItemType);
  EXPECT_STREQ(all_items[1]->GetItemType(), BirchFileItem::kItemType);
  EXPECT_STREQ(all_items[2]->GetItemType(), BirchWeatherItem::kItemType);
  EXPECT_FALSE(model->IsDataFresh());
}

TEST_F(BirchModelWithoutWeatherTest, MAYBE_DataFetchTimeout) {
  BirchModel* model = Shell::Get()->birch_model();
  TestModelConsumer consumer;
  EXPECT_TRUE(model);

  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), std::nullopt);

  // Passing time and setting data before requesting a birch data fetch will
  // not notify consumer.
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetCalendarItems({});

  EXPECT_TRUE(model->IsDataFresh());
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"0"));
  EXPECT_FALSE(model->IsDataFresh());
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  // Test that passing a short amount of time and setting some data does not
  // notify that items are ready.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  std::vector<BirchTabItem> tab_item_list;
  tab_item_list.emplace_back(u"tab title", GURL("example.com"),
                             base::Time::Now(), GURL("example.com/favicon_url"),
                             "session_name");
  model->SetRecentTabItems(tab_item_list);
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  // Test that passing enough time notifies that items are ready.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  std::vector<std::unique_ptr<BirchItem>> all_items = model->GetAllItems();
  EXPECT_EQ(all_items.size(), 2u);
  EXPECT_STREQ(all_items[0]->GetItemType(), BirchTabItem::kItemType);
  EXPECT_STREQ(all_items[1]->GetItemType(), BirchFileItem::kItemType);
  EXPECT_FALSE(model->IsDataFresh());
}

TEST_F(BirchModelWithoutWeatherTest, AddItemNotifiesCallback) {
  BirchModel* model = Shell::Get()->birch_model();
  TestModelConsumer consumer;
  EXPECT_TRUE(model);

  // Setting items in the model does not notify when no request has occurred.
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  // Make a data fetch request and set fresh tab data.
  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"0"));
  model->SetRecentTabItems(std::vector<BirchTabItem>());

  // Consumer is not notified until all data sources have responded.
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), std::nullopt);
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetWeatherItems({});
  model->SetCalendarItems({});

  // Adding file items sets all data as fresh, notifying consumers.
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  // Setting the file suggest items should not trigger items ready again, since
  // no data fetch was requested.
  file_item_list.emplace_back(base::FilePath("test path 1"), std::nullopt);
  file_item_list.emplace_back(base::FilePath("test path 2"), std::nullopt);
  model->SetFileSuggestItems(std::move(file_item_list));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  // Request another data fetch and expect the consumer to be notified once
  // items are set again.
  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"1"));
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::move(file_item_list));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0", "1"));
}

TEST_F(BirchModelTest, MultipleRequestsHaveIndependentTimeouts) {
  BirchModel* model = Shell::Get()->birch_model();
  TestModelConsumer consumer;
  EXPECT_TRUE(model);

  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"0"));

  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"1"));
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0", "1"));
  EXPECT_FALSE(model->IsDataFresh());

  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"2"));

  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0", "1"));

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  EXPECT_THAT(consumer.items_ready_responses(),
              testing::ElementsAre("0", "1", "2"));
  EXPECT_FALSE(model->IsDataFresh());
}

TEST_F(BirchModelTest, ResponseAfterFirstTimeout) {
  BirchModel* model = Shell::Get()->birch_model();
  TestModelConsumer consumer;
  EXPECT_TRUE(model);

  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"0"));

  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"1"));
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));
  EXPECT_FALSE(model->IsDataFresh());

  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), std::nullopt);
  model->SetFileSuggestItems(std::move(file_item_list));
  std::vector<BirchWeatherItem> weather_item_list;
  weather_item_list.emplace_back(u"cloudy", u"16 c", ui::ImageModel());
  model->SetWeatherItems(std::move(weather_item_list));
  std::vector<BirchTabItem> tab_item_list;
  tab_item_list.emplace_back(u"tab", GURL("foo.bar"), base::Time(),
                             GURL("favicon"), "session");
  model->SetRecentTabItems(std::move(tab_item_list));
  std::vector<BirchCalendarItem> calendar_item_list;
  calendar_item_list.emplace_back(u"Event 1", GURL(), base::Time(),
                                  base::Time());
  model->SetCalendarItems(std::move(calendar_item_list));
  EXPECT_TRUE(model->IsDataFresh());

  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0", "1"));
  EXPECT_EQ(model->GetAllItems().size(), 4u);

  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"2"));
  EXPECT_FALSE(model->IsDataFresh());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(model->IsDataFresh());
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0", "1"));

  model->SetFileSuggestItems({});
  model->SetWeatherItems({});
  model->SetRecentTabItems({});
  model->SetCalendarItems({});

  EXPECT_THAT(consumer.items_ready_responses(),
              testing::ElementsAre("0", "1", "2"));
  EXPECT_EQ(model->GetAllItems().size(), 0u);
  EXPECT_TRUE(model->IsDataFresh());
}

}  // namespace ash
