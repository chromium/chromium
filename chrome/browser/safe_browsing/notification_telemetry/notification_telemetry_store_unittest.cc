// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_store.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::EqualsProto;
using leveldb_proto::test::FakeDB;
using ::testing::_;
using ::testing::UnorderedPointwise;

namespace safe_browsing {

namespace {
CSBRR::ServiceWorkerBehavior MakeServiceWorkerRegistrationBehavior(
    const GURL& scope_url,
    const std::vector<GURL>& import_script_urls) {
  std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior =
      std::make_unique<CSBRR::ServiceWorkerBehavior>();

  service_worker_behavior->set_scope_url(scope_url.spec());

  for (const GURL& import_script_url : import_script_urls) {
    service_worker_behavior->add_import_script_urls(import_script_url.spec());
  }
  return *service_worker_behavior;
}
CSBRR::ServiceWorkerBehavior MakeServiceWorkerPushBehavior(
    const GURL& script_url,
    const std::vector<GURL>& requested_urls) {
  std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior =
      std::make_unique<CSBRR::ServiceWorkerBehavior>();

  service_worker_behavior->set_script_url(script_url.spec());

  for (const GURL& requested_url : requested_urls) {
    service_worker_behavior->add_requested_urls(requested_url.spec());
  }
  return *service_worker_behavior;
}

}  // namespace

class MockNotificationTelemetryStore : public NotificationTelemetryStore {
 public:
  explicit MockNotificationTelemetryStore(
      std::unique_ptr<ProtoDatabase<CSBRR::ServiceWorkerBehavior>>
          service_worker_behavior_db)
      : NotificationTelemetryStore(std::move(service_worker_behavior_db)) {}
  ~MockNotificationTelemetryStore() override = default;
};

class NotificationTelemetryStoreTest : public testing::Test {
 public:
  NotificationTelemetryStoreTest() = default;

  NotificationTelemetryStoreTest(const NotificationTelemetryStoreTest&) =
      delete;
  NotificationTelemetryStoreTest& operator=(
      const NotificationTelemetryStoreTest&) = delete;

  void SetUp() override {
    auto fake_db = std::make_unique<FakeDB<CSBRR::ServiceWorkerBehavior>>(
        &fake_db_entries_);
    fake_service_worker_behavior_db_ = fake_db.get();
    notification_telemetry_store_ =
        std::make_unique<MockNotificationTelemetryStore>(std::move(fake_db));
  }

  NotificationTelemetryStore* notification_telemetry_store() {
    return notification_telemetry_store_.get();
  }
  std::map<std::string, CSBRR::ServiceWorkerBehavior>& fake_db_entries() {
    return fake_db_entries_;
  }
  FakeDB<CSBRR::ServiceWorkerBehavior>* fake_service_worker_behavior_db() {
    return fake_service_worker_behavior_db_;
  }

  void VerifyServiceWorkerBehaviors(
      std::vector<CSBRR::ServiceWorkerBehavior>
          expected_service_worker_behaviors,
      bool success,
      std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>>
          service_worker_behaviors) {
    // Ignore ordering of `ServiceWorkerBehavior`.
    // `fake_db_entries` is expected to preserve ordering by insertion
    // time (key) but without ignoring the order, the test is flaky on some
    // builds.
    EXPECT_THAT(
        *service_worker_behaviors,
        UnorderedPointwise(EqualsProto(), expected_service_worker_behaviors));
  }

  void AddSuccessCallback(base::OnceClosure run_loop_closure, bool success) {
    store_op_success_ = success;
    std::move(run_loop_closure).Run();
  }

  void LoadEntriesCallback(
      base::OnceClosure run_loop_closure,
      bool success,
      std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>> entries) {
    store_op_success_ = success;
    entries_ = std::move(*entries);
    std::move(run_loop_closure).Run();
  }

  Profile* profile() { return &profile_; }
  MOCK_METHOD1(OnIsEmpty, void(bool));
  MOCK_METHOD1(OnDone, void(bool));
  MOCK_METHOD2(
      OnGetServiceWorkersBehaviors,
      void(bool, std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>>));

 protected:
  // Set to true during database callback when the operation is successful.
  bool store_op_success_;
  // ServiceWorkerBehaviors stored in the database populated by
  // `LoadEntriesCallback`.
  std::vector<CSBRR::ServiceWorkerBehavior> entries_;

 private:
  std::unique_ptr<NotificationTelemetryStore> notification_telemetry_store_;
  std::map<std::string, CSBRR::ServiceWorkerBehavior> fake_db_entries_;
  raw_ptr<FakeDB<CSBRR::ServiceWorkerBehavior>>
      fake_service_worker_behavior_db_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(NotificationTelemetryStoreTest, ConstructStoreFromProfile) {
  std::vector<GURL> requested_urls = {GURL("http://dest.com")};
  auto store = std::make_unique<NotificationTelemetryStore>(profile());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return store->IsInitializedForTest(); }));

  // Verify that the store functions as expected.
  base::RunLoop wait_for_add;
  store->AddServiceWorkerPushBehavior(
      GURL("http://script1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::AddSuccessCallback,
                     base::Unretained(this), wait_for_add.QuitClosure()));
  wait_for_add.Run();
  ASSERT_TRUE(store_op_success_);

  base::RunLoop wait_for_get;
  EXPECT_CALL(*this, OnDone(true));
  store->GetServiceWorkerBehaviors(
      base::BindOnce(&NotificationTelemetryStoreTest::LoadEntriesCallback,
                     base::Unretained(this), wait_for_get.QuitClosure()),
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  wait_for_get.Run();
  std::vector<CSBRR::ServiceWorkerBehavior> expected_service_worker_behaviors =
      {MakeServiceWorkerPushBehavior(GURL("http://script1.com"),
                                     requested_urls)};
  EXPECT_THAT(entries_, UnorderedPointwise(EqualsProto(),
                                           expected_service_worker_behaviors));
}

TEST_F(NotificationTelemetryStoreTest,
       QueueAddServiceWorkerBehaviorsWhileNotInitialized) {
  // Queue operation before the db is initialized.
  EXPECT_FALSE(notification_telemetry_store()->IsInitializedForTest());
  GURL script_url = GURL("http://script.com");
  std::vector<GURL> requested_urls = {GURL("http://dest.com")};
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      script_url, requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));

  // Initialize the db.
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_TRUE(notification_telemetry_store()->IsInitializedForTest());

  // Verify that the queued operation is executed after db is initialized.
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->UpdateCallback(true);
}

TEST_F(NotificationTelemetryStoreTest,
       QueueGetServiceWorkerBehaviorsWhileNotInitialized) {
  // Queue operation before the db is initialized.
  EXPECT_FALSE(notification_telemetry_store()->IsInitializedForTest());
  notification_telemetry_store()->GetServiceWorkerBehaviors(
      base::BindOnce(
          &NotificationTelemetryStoreTest::OnGetServiceWorkersBehaviors,
          base::Unretained(this)),
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));

  // Initialize the db.
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_TRUE(notification_telemetry_store()->IsInitializedForTest());

  // Verify that the queued operation is executed after db is initialized.
  EXPECT_CALL(*this, OnDone(true));
  EXPECT_CALL(*this, OnGetServiceWorkersBehaviors(true, _));
  fake_service_worker_behavior_db()->LoadCallback(true);
}

TEST_F(NotificationTelemetryStoreTest,
       QueueAddGetServiceWorkerBehaviorsWhileNotInitialized) {
  // Queue operations before the db is initialized.
  EXPECT_FALSE(notification_telemetry_store()->IsInitializedForTest());
  GURL script_url = GURL("http://script.com");
  std::vector<GURL> requested_urls = {GURL("http://dest.com")};
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      script_url, requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  notification_telemetry_store()->GetServiceWorkerBehaviors(
      base::BindOnce(
          &NotificationTelemetryStoreTest::OnGetServiceWorkersBehaviors,
          base::Unretained(this)),
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));

  // Initialize the db.
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_TRUE(notification_telemetry_store()->IsInitializedForTest());

  // Verify that the queued operations are executed after db is initialized.
  EXPECT_CALL(*this, OnDone(true)).Times(2);
  EXPECT_CALL(*this, OnGetServiceWorkersBehaviors(true, _));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  fake_service_worker_behavior_db()->LoadCallback(true);
}

TEST_F(NotificationTelemetryStoreTest, QueueDeleteAllWhileNotInitialized) {
  // Queue operation before the db is initialized.
  EXPECT_FALSE(notification_telemetry_store()->IsInitializedForTest());
  notification_telemetry_store()->DeleteAll(base::BindOnce(
      &NotificationTelemetryStoreTest::OnDone, base::Unretained(this)));

  // Initialize the db.
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_TRUE(notification_telemetry_store()->IsInitializedForTest());

  //  Verify that the queued operation is executed after db is initialized.
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->UpdateCallback(true);
}

TEST_F(NotificationTelemetryStoreTest, AddServiceWorkerBehaviorsToDb) {
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  std::vector<GURL> requested_urls = {GURL("http://dest.com")};

  // Validate the database is empty to begin with.
  ASSERT_EQ(static_cast<size_t>(0), fake_db_entries().size());

  // Validate SuccessCallback is called when updating the database.
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      GURL("http://script1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      GURL("http://script2.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->UpdateCallback(true);

  // Validate the database is populated.
  ASSERT_EQ(static_cast<size_t>(2), fake_db_entries().size());
}

TEST_F(NotificationTelemetryStoreTest, VerifyAddedServiceWorkerPushBehaviors) {
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  GURL requested_url = GURL("http://dest.com");
  GURL script_url_1 = GURL("http://script1.com");
  GURL script_url_2 = GURL("http://script2.com");
  std::vector<GURL> requested_urls = {requested_url};
  std::vector<CSBRR::ServiceWorkerBehavior> expected_service_worker_behaviors =
      {MakeServiceWorkerPushBehavior(script_url_1, requested_urls),
       MakeServiceWorkerPushBehavior(script_url_2, requested_urls)};

  // Validate the database is empty to begin with.
  ASSERT_EQ(static_cast<size_t>(0), fake_db_entries().size());

  // Add entries to the database.
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      script_url_1, requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      script_url_2, requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);

  // Validate the entries are as expected.
  notification_telemetry_store()->GetServiceWorkerBehaviors(
      base::BindOnce(
          &NotificationTelemetryStoreTest::VerifyServiceWorkerBehaviors,
          base::Unretained(this), std::move(expected_service_worker_behaviors)),
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->LoadCallback(true);
}

TEST_F(NotificationTelemetryStoreTest,
       VerifyAddedServiceWorkerRegistrationBehaviors) {
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  GURL scope_url_1 = GURL("http://scope1.com");
  GURL scope_url_2 = GURL("http://scope2.com");
  std::vector<GURL> import_script_urls = {GURL("http://dest1.com"),
                                          GURL("http://dest2.com")};
  std::vector<CSBRR::ServiceWorkerBehavior> expected_service_worker_behaviors =
      {MakeServiceWorkerRegistrationBehavior(scope_url_1, import_script_urls),
       MakeServiceWorkerRegistrationBehavior(scope_url_2, import_script_urls)};

  // Validate the database is empty to begin with.
  ASSERT_EQ(static_cast<size_t>(0), fake_db_entries().size());

  // Add entries to the database.
  notification_telemetry_store()->AddServiceWorkerRegistrationBehavior(
      scope_url_1, import_script_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerRegistrationBehavior(
      scope_url_2, import_script_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);

  // Validate the entries are as expected.
  notification_telemetry_store()->GetServiceWorkerBehaviors(
      base::BindOnce(
          &NotificationTelemetryStoreTest::VerifyServiceWorkerBehaviors,
          base::Unretained(this), std::move(expected_service_worker_behaviors)),
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->LoadCallback(true);
}

TEST_F(NotificationTelemetryStoreTest, GetAllEntries) {
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  std::vector<GURL> requested_urls = {GURL("http://dest.com")};

  // Validate the database is empty to begin with.
  ASSERT_EQ(static_cast<size_t>(0), fake_db_entries().size());

  // Populate the database.
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      GURL("http://script1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      GURL("http://scirpt2.com"), requested_urls,

      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);

  // Validate the database is populated.
  ASSERT_EQ(static_cast<size_t>(2), fake_db_entries().size());

  // Query the database.
  notification_telemetry_store()->GetServiceWorkerBehaviors(
      base::BindOnce(
          &NotificationTelemetryStoreTest::OnGetServiceWorkersBehaviors,
          base::Unretained(this)),
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  EXPECT_CALL(*this, OnGetServiceWorkersBehaviors(true, _));
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->LoadCallback(true);
}

TEST_F(NotificationTelemetryStoreTest, DeleteAllEntries) {
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  std::vector<GURL> requested_urls = {GURL("http://dest.com")};

  // Validate the database is empty to begin with.
  ASSERT_EQ(static_cast<size_t>(0), fake_db_entries().size());

  // Populate the database.
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      GURL("http://script1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      GURL("http://script2.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);

  // Validate the database is populated.
  ASSERT_EQ(static_cast<size_t>(2), fake_db_entries().size());

  // Clear the database.
  notification_telemetry_store()->DeleteAll(base::BindOnce(
      &NotificationTelemetryStoreTest::OnDone, base::Unretained(this)));
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->UpdateCallback(true);

  // Validate the database is empty.
  ASSERT_EQ(static_cast<size_t>(0), fake_db_entries().size());
}

TEST_F(NotificationTelemetryStoreTest, FailedInitialization) {
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kError);

  // Expect the callback isn't run.
  EXPECT_CALL(*this, OnDone(true)).Times(0);

  // Attempt to populate the database but fail to because it is not initialized.
  std::vector<GURL> requested_urls = {GURL("http://dest.com")};
  notification_telemetry_store()->AddServiceWorkerPushBehavior(
      GURL("http://script1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
}

}  // namespace safe_browsing
