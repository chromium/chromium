// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/desk_ash.h"

#include <memory>

#include "ash/wm/desks/desk.h"
#include "base/guid.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chromeos/crosapi/mojom/desk.mojom-forward.h"
#include "chromeos/crosapi/mojom/desk.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace crosapi {

class MockDesksClient : public DesksClient {
 public:
  MOCK_METHOD((base::expected<const ash::Desk*, DesksClient::DeskActionError>),
              GetDeskByID,
              (const base::GUID&),
              (const));
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

 protected:
  content::BrowserTaskEnvironment task_environment_;
  mojo::Remote<mojom::Desk> desk_ash_remote_;

 private:
  testing::NiceMock<MockDesksClient> mock_desks_client_;
  std::unique_ptr<DeskAsh> desk_ash_;
};

TEST_F(DeskAshTest, GetDeskByIDWithInvalidIDTest) {
  ASSERT_EQ(&mock_desks_client(), DesksClient::Get());
  base::GUID fake_id;
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

}  // namespace crosapi
