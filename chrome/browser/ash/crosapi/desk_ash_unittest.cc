// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_ash.h"

#include <memory>

#include "ash/wm/desks/desk.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chromeos/crosapi/mojom/desk.mojom-forward.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace crosapi {

class MockDesksClient : public DesksClient {
 public:
  MOCK_METHOD((base::expected<const ash::Desk*, DesksClient::DeskActionError>),
              GetDeskByID,
              (const base::Uuid&),
              (const));
};

class TestDeskEventObserver : public crosapi::mojom::DeskEventObserver {
 public:
  void OnDeskSwitched(const base::Uuid& new_desk_id,
                      const base::Uuid& previous_desk_id) override {
    event_future_.AddValue(new_desk_id);
    event_future_.AddValue(previous_desk_id);
  }
  void OnDeskAdded(const base::Uuid& new_desk_id, bool from_undo) override {
    event_future_.AddValue(new_desk_id);
  }
  void OnDeskRemoved(const base::Uuid& removed_desk_id) override {
    event_future_.AddValue(removed_desk_id);
  }

  mojo::PendingRemote<crosapi::mojom::DeskEventObserver> GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<crosapi::mojom::DeskEventObserver>& GetReceiver() {
    return receiver_;
  }
  base::Uuid WaitAndGet() { return event_future_.Take(); }

 private:
  mojo::Receiver<crosapi::mojom::DeskEventObserver> receiver_{this};
  base::test::RepeatingTestFuture<base::Uuid> event_future_;
};

class DeskAshTest : public testing::Test {
 public:
  DeskAshTest() = default;
  ~DeskAshTest() override = default;

  void SetUp() override {
    desk_ash_ = std::make_unique<DeskAsh>();
    desk_ash_->BindReceiver(desk_ash_remote_.BindNewPipeAndPassReceiver());
  }

  MockDesksClient& mock_desks_client() { return mock_desks_client_; }
  TestDeskEventObserver& desk_event_observer() { return desk_event_observer_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  mojo::Remote<mojom::Desk> desk_ash_remote_;
  std::unique_ptr<DeskAsh> desk_ash_;

 private:
  testing::NiceMock<MockDesksClient> mock_desks_client_;
  TestDeskEventObserver desk_event_observer_;
};

TEST_F(DeskAshTest, GetDeskByIDWithInvalidIDTest) {
  ASSERT_EQ(&mock_desks_client(), DesksClient::Get());
  base::Uuid fake_id;
  EXPECT_CALL(mock_desks_client(), GetDeskByID(fake_id))
      .Times(1)
      .WillOnce(testing::Return(
          base::unexpected(DesksClient::DeskActionError::kInvalidIdError)));

  base::test::TestFuture<mojom::GetDeskByIDResultPtr> future;
  desk_ash_remote_->GetDeskByID(fake_id, future.GetCallback());
  auto result = future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(crosapi::mojom::DeskCrosApiError::kInvalidIdError,
            result->get_error());
}

TEST_F(DeskAshTest, NotifyDeskAddedTest) {
  desk_ash_remote_->AddDeskEventObserver(desk_event_observer().GetRemote());
  // Flush pipe so that registration shows up.
  desk_ash_remote_.FlushForTesting();
  desk_event_observer().GetReceiver().FlushForTesting();
  auto desk_id(base::Uuid::GenerateRandomV4());
  desk_ash_->NotifyDeskAdded(desk_id);

  EXPECT_EQ(desk_event_observer().WaitAndGet(), desk_id);
}

TEST_F(DeskAshTest, NotifyDeskRemovedTest) {
  desk_ash_remote_->AddDeskEventObserver(desk_event_observer().GetRemote());
  // Flush pipe so that registration shows up.
  desk_ash_remote_.FlushForTesting();
  desk_event_observer().GetReceiver().FlushForTesting();
  auto desk_id(base::Uuid::GenerateRandomV4());
  desk_ash_->NotifyDeskRemoved(desk_id);

  EXPECT_EQ(desk_event_observer().WaitAndGet(), desk_id);
}

TEST_F(DeskAshTest, NotifyDeskSwitchedTest) {
  desk_ash_remote_->AddDeskEventObserver(desk_event_observer().GetRemote());
  // Flush pipe so that registration shows up.
  desk_ash_remote_.FlushForTesting();
  desk_event_observer().GetReceiver().FlushForTesting();
  auto old_id(base::Uuid::GenerateRandomV4());
  auto new_id(base::Uuid::GenerateRandomV4());

  desk_ash_->NotifyDeskSwitched(new_id, old_id);

  EXPECT_EQ(desk_event_observer().WaitAndGet(), new_id);
  EXPECT_EQ(desk_event_observer().WaitAndGet(), old_id);
}

TEST_F(DeskAshTest, NotifyDeskRemovalUndoneTest) {
  desk_ash_remote_->AddDeskEventObserver(desk_event_observer().GetRemote());
  // Flush pipe so that registration shows up.
  desk_ash_remote_.FlushForTesting();
  desk_event_observer().GetReceiver().FlushForTesting();
  auto desk_id(base::Uuid::GenerateRandomV4());
  desk_ash_->NotifyDeskAdded(desk_id, true);

  EXPECT_EQ(desk_event_observer().WaitAndGet(), desk_id);
}

}  // namespace crosapi
