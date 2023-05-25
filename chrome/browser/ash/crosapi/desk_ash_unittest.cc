// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_ash.h"

#include <memory>

#include "ash/wm/desks/desk.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chromeos/crosapi/mojom/desk.mojom-forward.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  void OnDeskSwitched(const base::GUID& new_desk_id,
                      const base::GUID& previous_desk_id) override {}
  void OnDeskAdded(const base::GUID& new_desk_id) override {}
  void OnDeskRemoved(const base::GUID& removed_desk_id) override {}

  mojo::PendingRemote<crosapi::mojom::DeskEventObserver> GetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<crosapi::mojom::DeskEventObserver> receiver_{this};
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

 private:
  testing::NiceMock<MockDesksClient> mock_desks_client_;
  TestDeskEventObserver desk_event_observer_;
  std::unique_ptr<DeskAsh> desk_ash_;
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

TEST_F(DeskAshTest, AddDeskEventObserversTest) {
  desk_ash_remote_->AddDeskEventObserver(desk_event_observer().GetRemote());
}

}  // namespace crosapi
