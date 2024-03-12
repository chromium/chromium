// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_model.h"

#include <optional>

#include "ash/birch/birch_data_provider.h"
#include "ash/birch/birch_item.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// A data provider that does nothing.
class StubBirchDataProvider : public BirchDataProvider {
 public:
  StubBirchDataProvider() = default;
  ~StubBirchDataProvider() override = default;

  // BirchDataProvider:
  void RequestBirchDataFetch() override {}
};

// A BirchClient that returns data providers that do nothing.
class StubBirchClient : public BirchClient {
 public:
  StubBirchClient() = default;
  ~StubBirchClient() override = default;

  // BirchClient:
  BirchDataProvider* GetCalendarProvider() override { return &data_provider_; }
  BirchDataProvider* GetFileSuggestProvider() override {
    return &data_provider_;
  }
  BirchDataProvider* GetRecentTabsProvider() override {
    return &data_provider_;
  }
  BirchDataProvider* GetReleaseNotesProvider() override {
    return &data_provider_;
  }

 private:
  StubBirchDataProvider data_provider_;
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

base::Time TimeFromString(const char* time_string) {
  base::Time time;
  CHECK(base::Time::FromString(time_string, &time));
  return time;
}

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
        std::make_unique<StubBirchDataProvider>());
    Shell::Get()->birch_model()->SetClient(&stub_birch_client_);

    // Set a test clock so that ranking uses a consistent time across test runs.
    test_clock_.SetNow(TimeFromString("22 Feb 2024 4:00 UTC"));
    Shell::Get()->birch_model()->OverrideClockForTest(&test_clock_);
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClient(nullptr);
    AshTestBase::TearDown();
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  StubBirchClient stub_birch_client_;
  base::SimpleTestClock test_clock_;
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
  model->SetAttachmentItems(std::vector<BirchAttachmentItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetReleaseNotesItems(std::vector<BirchReleaseNotesItem>());
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  // Make a data fetch request and set fresh tab data.
  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"0"));
  model->SetRecentTabItems(std::vector<BirchTabItem>());

  // Consumer is not notified until all data sources have responded.
  EXPECT_THAT(consumer.items_ready_responses(), testing::IsEmpty());

  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetWeatherItems({});
  model->SetCalendarItems({});
  model->SetAttachmentItems({});
  model->SetReleaseNotesItems({});

  // Adding file items sets all data as fresh, notifying consumers.
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  // Setting the file suggest items should not trigger items ready again, since
  // no data fetch was requested.
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  file_item_list.emplace_back(base::FilePath("test path 2"), base::Time());
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
  model->SetAttachmentItems({});
  model->SetReleaseNotesItems({});

  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0", "1"));
}

TEST_F(BirchModelTest, DataFetchForNonPrimaryUserClearsModel) {
  BirchModel* model = Shell::Get()->birch_model();
  TestModelConsumer consumer;

  // Sign in to a secondary user.
  SimulateUserLogin("user2@test.com");
  ASSERT_FALSE(Shell::Get()->session_controller()->IsUserPrimary());

  // Add an item to the model.
  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  model->SetFileSuggestItems(std::move(file_item_list));

  // Request a data fetch.
  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"0"));
  // The fetch callback was called.
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  // The model is empty.
  EXPECT_TRUE(model->GetAllItems().empty());
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
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  std::vector<BirchWeatherItem> weather_items;
  weather_items.emplace_back(u"desc", u"temp", ui::ImageModel());
  model->SetWeatherItems(std::move(weather_items));
  model->SetCalendarItems({});
  model->SetAttachmentItems({});
  model->SetReleaseNotesItems({});

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
                             "session_name",
                             BirchTabItem::DeviceFormFactor::kDesktop);
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
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());

  // Passing time and setting data before requesting a birch data fetch will
  // not notify consumer.
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetCalendarItems({});
  model->SetAttachmentItems({});
  model->SetReleaseNotesItems({});

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
                             "session_name",
                             BirchTabItem::DeviceFormFactor::kDesktop);
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
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetWeatherItems({});
  model->SetCalendarItems({});
  model->SetAttachmentItems({});
  model->SetReleaseNotesItems({});

  // Adding file items sets all data as fresh, notifying consumers.
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  // Setting the file suggest items should not trigger items ready again, since
  // no data fetch was requested.
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  file_item_list.emplace_back(base::FilePath("test path 2"), base::Time());
  model->SetFileSuggestItems(std::move(file_item_list));
  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0"));

  // Request another data fetch and expect the consumer to be notified once
  // items are set again.
  model->RequestBirchDataFetch(base::BindOnce(&TestModelConsumer::OnItemsReady,
                                              base::Unretained(&consumer),
                                              /*id=*/"1"));
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::move(file_item_list));
  model->SetCalendarItems({});
  model->SetAttachmentItems({});
  model->SetReleaseNotesItems({});
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
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  model->SetFileSuggestItems(std::move(file_item_list));
  std::vector<BirchWeatherItem> weather_item_list;
  weather_item_list.emplace_back(u"cloudy", u"16 c", ui::ImageModel());
  model->SetWeatherItems(std::move(weather_item_list));
  std::vector<BirchTabItem> tab_item_list;
  tab_item_list.emplace_back(u"tab", GURL("foo.bar"), base::Time(),
                             GURL("favicon"), "session",
                             BirchTabItem::DeviceFormFactor::kDesktop);
  model->SetRecentTabItems(std::move(tab_item_list));
  std::vector<BirchCalendarItem> calendar_item_list;
  calendar_item_list.emplace_back(u"Event 1");
  model->SetCalendarItems(std::move(calendar_item_list));
  std::vector<BirchAttachmentItem> attachment_item_list;
  attachment_item_list.emplace_back(u"Attachment 1");
  model->SetAttachmentItems(std::move(attachment_item_list));
  std::vector<BirchReleaseNotesItem> release_notes_item_list;
  release_notes_item_list.emplace_back(u"note", 1, u"explore", GURL("foo.bar"),
                                       base::Time());
  model->SetReleaseNotesItems(release_notes_item_list);

  EXPECT_TRUE(model->IsDataFresh());

  EXPECT_THAT(consumer.items_ready_responses(), testing::ElementsAre("0", "1"));
  EXPECT_EQ(model->GetAllItems().size(), 6u);

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
  model->SetAttachmentItems({});
  model->SetReleaseNotesItems({});

  EXPECT_THAT(consumer.items_ready_responses(),
              testing::ElementsAre("0", "1", "2"));
  EXPECT_EQ(model->GetAllItems().size(), 0u);
  EXPECT_TRUE(model->IsDataFresh());
}

TEST_F(BirchModelTest, GetAllItems) {
  BirchModel* model = Shell::Get()->birch_model();

  // Insert one item of each type.
  std::vector<BirchWeatherItem> weather_item_list;
  weather_item_list.emplace_back(u"cloudy", u"16 c", ui::ImageModel());
  model->SetWeatherItems(std::move(weather_item_list));
  std::vector<BirchReleaseNotesItem> release_notes_item_list;
  release_notes_item_list.emplace_back(u"note", 1, u"explore", GURL("foo.bar"),
                                       base::Time());
  model->SetReleaseNotesItems(std::move(release_notes_item_list));
  std::vector<BirchCalendarItem> calendar_item_list;
  calendar_item_list.emplace_back(u"Event 1");
  model->SetCalendarItems(std::move(calendar_item_list));
  std::vector<BirchAttachmentItem> attachment_item_list;
  attachment_item_list.emplace_back(u"Attachment 1");
  model->SetAttachmentItems(std::move(attachment_item_list));
  std::vector<BirchTabItem> tab_item_list;
  tab_item_list.emplace_back(u"tab", GURL("foo.bar"), base::Time(),
                             GURL("favicon"), "session",
                             BirchTabItem::DeviceFormFactor::kDesktop);
  model->SetRecentTabItems(std::move(tab_item_list));
  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  model->SetFileSuggestItems(std::move(file_item_list));

  // Verify that GetAllItems() returns the correct number of items and the
  // code didn't skip a type.
  std::vector<std::unique_ptr<BirchItem>> all_items = model->GetAllItems();
  ASSERT_EQ(all_items.size(), 6u);
  EXPECT_STREQ(all_items[0]->GetItemType(), BirchReleaseNotesItem::kItemType);
  EXPECT_STREQ(all_items[1]->GetItemType(), BirchCalendarItem::kItemType);
  EXPECT_STREQ(all_items[2]->GetItemType(), BirchAttachmentItem::kItemType);
  EXPECT_STREQ(all_items[3]->GetItemType(), BirchTabItem::kItemType);
  EXPECT_STREQ(all_items[4]->GetItemType(), BirchFileItem::kItemType);
  EXPECT_STREQ(all_items[5]->GetItemType(), BirchWeatherItem::kItemType);
}

TEST_F(BirchModelTest, GetItemsForDisplay_EnoughTypes) {
  BirchModel* model = Shell::Get()->birch_model();

  // Insert one item of each type.
  std::vector<BirchCalendarItem> calendar_item_list;
  calendar_item_list.emplace_back(u"Event 1");
  calendar_item_list.back().ranking = 5.f;
  model->SetCalendarItems(std::move(calendar_item_list));

  std::vector<BirchAttachmentItem> attachment_item_list;
  attachment_item_list.emplace_back(u"Attachment 1");
  attachment_item_list.back().ranking = 4.f;
  model->SetAttachmentItems(std::move(attachment_item_list));

  std::vector<BirchTabItem> tab_item_list;
  tab_item_list.emplace_back(u"tab", GURL("foo.bar"), base::Time(),
                             GURL("favicon"), "session",
                             BirchTabItem::DeviceFormFactor::kDesktop);
  tab_item_list.back().ranking = 3.f;
  model->SetRecentTabItems(std::move(tab_item_list));

  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  file_item_list.back().ranking = 2.f;
  model->SetFileSuggestItems(std::move(file_item_list));

  std::vector<BirchWeatherItem> weather_item_list;
  weather_item_list.emplace_back(u"cloudy", u"16 c", ui::ImageModel());
  weather_item_list.back().ranking = 1.f;
  model->SetWeatherItems(std::move(weather_item_list));

  std::vector<std::unique_ptr<BirchItem>> items = model->GetItemsForDisplay();

  // The maximum of 4 items are returned.
  ASSERT_EQ(items.size(), 4u);

  // The items are in priority order.
  EXPECT_FLOAT_EQ(items[0]->ranking, 1.f);
  EXPECT_STREQ(items[0]->GetItemType(), BirchWeatherItem::kItemType);
  EXPECT_FLOAT_EQ(items[1]->ranking, 2.f);
  EXPECT_STREQ(items[1]->GetItemType(), BirchFileItem::kItemType);
  EXPECT_FLOAT_EQ(items[2]->ranking, 3.f);
  EXPECT_STREQ(items[2]->GetItemType(), BirchTabItem::kItemType);
  EXPECT_FLOAT_EQ(items[3]->ranking, 4.f);
  EXPECT_STREQ(items[3]->GetItemType(), BirchAttachmentItem::kItemType);
}

TEST_F(BirchModelTest, GetItemsForDisplay_IncludesDuplicateTypes) {
  BirchModel* model = Shell::Get()->birch_model();

  // Insert 2 calendar events with high priority.
  std::vector<BirchCalendarItem> calendar_item_list;
  calendar_item_list.emplace_back(u"Event 1");
  calendar_item_list.back().ranking = 1.f;
  calendar_item_list.emplace_back(u"Event 2");
  calendar_item_list.back().ranking = 2.f;
  model->SetCalendarItems(std::move(calendar_item_list));

  // Then insert 3 other items with lower priority.
  std::vector<BirchAttachmentItem> attachment_item_list;
  attachment_item_list.emplace_back(u"Attachment 1");
  attachment_item_list.back().ranking = 3.f;
  model->SetAttachmentItems(std::move(attachment_item_list));

  std::vector<BirchTabItem> tab_item_list;
  tab_item_list.emplace_back(u"tab", GURL("foo.bar"), base::Time(),
                             GURL("favicon"), "session",
                             BirchTabItem::DeviceFormFactor::kDesktop);
  tab_item_list.back().ranking = 4.f;
  model->SetRecentTabItems(std::move(tab_item_list));

  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  file_item_list.back().ranking = 5.f;
  model->SetFileSuggestItems(std::move(file_item_list));

  std::vector<std::unique_ptr<BirchItem>> items = model->GetItemsForDisplay();

  // The maximum of 4 items are returned.
  ASSERT_EQ(items.size(), 4u);

  // Both calendar events are included.
  EXPECT_FLOAT_EQ(items[0]->ranking, 1.f);
  EXPECT_STREQ(items[0]->GetItemType(), BirchCalendarItem::kItemType);
  EXPECT_FLOAT_EQ(items[1]->ranking, 2.f);
  EXPECT_STREQ(items[1]->GetItemType(), BirchCalendarItem::kItemType);
  EXPECT_FLOAT_EQ(items[2]->ranking, 3.f);
  EXPECT_STREQ(items[2]->GetItemType(), BirchAttachmentItem::kItemType);
  EXPECT_FLOAT_EQ(items[3]->ranking, 4.f);
  EXPECT_STREQ(items[3]->GetItemType(), BirchTabItem::kItemType);
}

TEST_F(BirchModelTest, GetItemsForDisplay_TwoDuplicateTypes) {
  BirchModel* model = Shell::Get()->birch_model();

  // Insert 2 items of the same type.
  std::vector<BirchCalendarItem> calendar_item_list;
  calendar_item_list.emplace_back(u"Event 1");
  calendar_item_list.back().ranking = 1.f;
  calendar_item_list.emplace_back(u"Event 2");
  calendar_item_list.back().ranking = 2.f;
  model->SetCalendarItems(std::move(calendar_item_list));

  // Insert 2 more items of a different type.
  std::vector<BirchAttachmentItem> attachment_item_list;
  attachment_item_list.emplace_back(u"Attachment 1");
  attachment_item_list.back().ranking = 3.f;
  attachment_item_list.emplace_back(u"Attachment 2");
  attachment_item_list.back().ranking = 4.f;
  model->SetAttachmentItems(std::move(attachment_item_list));

  std::vector<std::unique_ptr<BirchItem>> items = model->GetItemsForDisplay();

  ASSERT_EQ(items.size(), 4u);
  EXPECT_FLOAT_EQ(items[0]->ranking, 1.f);
  EXPECT_STREQ(items[0]->GetItemType(), BirchCalendarItem::kItemType);
  EXPECT_FLOAT_EQ(items[1]->ranking, 2.f);
  EXPECT_STREQ(items[1]->GetItemType(), BirchCalendarItem::kItemType);
  EXPECT_FLOAT_EQ(items[2]->ranking, 3.f);
  EXPECT_STREQ(items[2]->GetItemType(), BirchAttachmentItem::kItemType);
  EXPECT_FLOAT_EQ(items[3]->ranking, 4.f);
  EXPECT_STREQ(items[3]->GetItemType(), BirchAttachmentItem::kItemType);
}

TEST_F(BirchModelTest, GetItemsForDisplay_NotEnoughItems) {
  BirchModel* model = Shell::Get()->birch_model();

  // Insert 3 items of the same type.
  std::vector<BirchCalendarItem> calendar_item_list;
  calendar_item_list.emplace_back(u"Event 1");
  calendar_item_list.back().ranking = 1.f;
  calendar_item_list.emplace_back(u"Event 2");
  calendar_item_list.back().ranking = 2.f;
  calendar_item_list.emplace_back(u"Event 3");
  calendar_item_list.back().ranking = 3.f;
  model->SetCalendarItems(std::move(calendar_item_list));

  std::vector<std::unique_ptr<BirchItem>> items = model->GetItemsForDisplay();

  // 3 items are returned.
  ASSERT_EQ(items.size(), 3u);
  EXPECT_FLOAT_EQ(items[0]->ranking, 1.f);
  EXPECT_STREQ(items[0]->GetItemType(), BirchCalendarItem::kItemType);
  EXPECT_FLOAT_EQ(items[1]->ranking, 2.f);
  EXPECT_STREQ(items[1]->GetItemType(), BirchCalendarItem::kItemType);
  EXPECT_FLOAT_EQ(items[2]->ranking, 3.f);
  EXPECT_STREQ(items[2]->GetItemType(), BirchCalendarItem::kItemType);
}

TEST_F(BirchModelTest, GetItemsForDisplay_NotRankedItem) {
  BirchModel* model = Shell::Get()->birch_model();

  // Insert 1 regular item and 1 item with no ranking.
  std::vector<BirchCalendarItem> calendar_item_list;
  calendar_item_list.emplace_back(u"Ranked");
  calendar_item_list.back().ranking = 1.f;
  calendar_item_list.emplace_back(u"Unranked");
  model->SetCalendarItems(std::move(calendar_item_list));

  std::vector<std::unique_ptr<BirchItem>> items = model->GetItemsForDisplay();

  // Only 1 item is returned because the unranked item is discarded.
  ASSERT_EQ(items.size(), 1u);
  EXPECT_FLOAT_EQ(items[0]->ranking, 1.f);
  EXPECT_STREQ(items[0]->GetItemType(), BirchCalendarItem::kItemType);
}

TEST_F(BirchModelTest, ModelClearedOnMultiProfileUserSwitch) {
  BirchModel* model = Shell::Get()->birch_model();
  TestModelConsumer consumer;

  // Add an item to the model.
  std::vector<BirchFileItem> file_item_list;
  file_item_list.emplace_back(base::FilePath("test path 1"), base::Time());
  model->SetFileSuggestItems(std::move(file_item_list));

  // Set the other types as empty so the model has fresh data.
  model->SetCalendarItems({});
  model->SetAttachmentItems({});
  model->SetRecentTabItems({});
  model->SetWeatherItems({});
  model->SetReleaseNotesItems({});
  ASSERT_TRUE(model->IsDataFresh());

  // Sign in to a secondary user.
  SimulateUserLogin("user2@test.com");
  ASSERT_FALSE(Shell::Get()->session_controller()->IsUserPrimary());

  // The model is empty.
  EXPECT_TRUE(model->GetAllItems().empty());

  // The data is not fresh.
  EXPECT_FALSE(model->IsDataFresh());
}

TEST_F(BirchModelTest, WeatherItemsClearedWhenGeolocationDisabled) {
  BirchModel* model = Shell::Get()->birch_model();

  // Geolocation starts as allowed.
  auto* geolocation_provider = SimpleGeolocationProvider::GetInstance();
  ASSERT_EQ(geolocation_provider->GetGeolocationAccessLevel(),
            GeolocationAccessLevel::kAllowed);

  // Add a weather item.
  std::vector<BirchWeatherItem> weather_items;
  weather_items.emplace_back(u"Sunny", u"72", ui::ImageModel());
  model->SetWeatherItems(std::move(weather_items));
  ASSERT_FALSE(model->GetWeatherForTest().empty());

  // Disable geolocation permission.
  geolocation_provider->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kDisallowed);

  // The weather item is removed.
  EXPECT_TRUE(model->GetWeatherForTest().empty());
}

}  // namespace ash
