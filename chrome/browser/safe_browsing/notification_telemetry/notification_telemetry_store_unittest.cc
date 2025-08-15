// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_store.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/protobuf_matchers.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::EqualsProto;
using leveldb_proto::test::FakeDB;
using ::testing::_;

namespace safe_browsing {

namespace {
CSBRR::ServiceWorkerBehavior MakeServiceWorkerBehavior(
    const GURL& scope_url,
    const std::vector<GURL>& requested_urls) {
  std::unique_ptr<CSBRR::ServiceWorkerBehavior> service_worker_behavior =
      std::make_unique<CSBRR::ServiceWorkerBehavior>();
  service_worker_behavior->set_scope_url(scope_url.spec());
  for (const GURL& requested_url : requested_urls) {
    service_worker_behavior->add_requested_urls(requested_url.spec());
  }
  return *service_worker_behavior;
}

}  // namespace

// TODO(crbug.com/438234675): Add coverage for prod constructor.
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
    fake_service_worker_behavor_db_ = fake_db.get();
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
    return fake_service_worker_behavor_db_;
  }

  void VerifyServiceWorkerBehaviors(
      std::vector<CSBRR::ServiceWorkerBehavior>
          expected_service_worker_behaviors,
      bool success,
      std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>>
          service_worker_behaviors) {
    ASSERT_EQ(expected_service_worker_behaviors.size(),
              service_worker_behaviors->size());
    for (size_t i = 0; i < expected_service_worker_behaviors.size(); i++) {
      EXPECT_THAT((*service_worker_behaviors)[i],
                  EqualsProto(expected_service_worker_behaviors[i]));
    }
  }
  MOCK_METHOD1(OnIsEmpty, void(bool));
  MOCK_METHOD1(OnDone, void(bool));
  MOCK_METHOD2(
      OnGetServiceWorkersBehaviors,
      void(bool, std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>>));

 private:
  std::unique_ptr<NotificationTelemetryStore> notification_telemetry_store_;
  std::map<std::string, CSBRR::ServiceWorkerBehavior> fake_db_entries_;
  raw_ptr<FakeDB<CSBRR::ServiceWorkerBehavior>> fake_service_worker_behavor_db_;
};

TEST_F(NotificationTelemetryStoreTest, AddServiceWorkerBehaviorsToDb) {
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  std::vector<GURL> requested_urls = {GURL("http://dest.com")};

  // Validate the database is empty to begin with.
  ASSERT_EQ(static_cast<size_t>(0), fake_db_entries().size());

  // Validate SuccessCallback is called when updating the database.
  notification_telemetry_store()->AddServiceWorkerBehavior(
      GURL("http://scope1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerBehavior(
      GURL("http://scope2.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  EXPECT_CALL(*this, OnDone(true));
  fake_service_worker_behavior_db()->UpdateCallback(true);

  // Validate the database is populated.
  ASSERT_EQ(static_cast<size_t>(2), fake_db_entries().size());
}

TEST_F(NotificationTelemetryStoreTest, VerifyAddedServiceWorkerBehaviors) {
  fake_service_worker_behavior_db()->InitStatusCallback(
      leveldb_proto::Enums::InitStatus::kOK);
  GURL requested_url = GURL("http://dest.com");
  GURL scope_url_1 = GURL("http://scope1.com");
  GURL scope_url_2 = GURL("http://scope2.com");
  std::vector<GURL> requested_urls = {requested_url};
  std::vector<CSBRR::ServiceWorkerBehavior> expected_service_worker_behaviors =
      {MakeServiceWorkerBehavior(scope_url_1, requested_urls),
       MakeServiceWorkerBehavior(scope_url_2, requested_urls)};

  // Validate the database is empty to begin with.
  ASSERT_EQ(static_cast<size_t>(0), fake_db_entries().size());

  // Add entries to the database.
  notification_telemetry_store()->AddServiceWorkerBehavior(
      scope_url_1, requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerBehavior(
      scope_url_2, requested_urls,
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
  notification_telemetry_store()->AddServiceWorkerBehavior(
      GURL("http://scope1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerBehavior(
      GURL("http://scope2.com"), requested_urls,
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
  notification_telemetry_store()->AddServiceWorkerBehavior(
      GURL("http://scope1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
  fake_service_worker_behavior_db()->UpdateCallback(true);
  notification_telemetry_store()->AddServiceWorkerBehavior(
      GURL("http://scope2.com"), requested_urls,
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
  notification_telemetry_store()->AddServiceWorkerBehavior(
      GURL("http://scope1.com"), requested_urls,
      base::BindOnce(&NotificationTelemetryStoreTest::OnDone,
                     base::Unretained(this)));
}

}  // namespace safe_browsing
