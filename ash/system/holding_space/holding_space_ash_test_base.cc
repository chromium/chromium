// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_ash_test_base.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/strings/strcat.h"

namespace ash {

HoldingSpaceAshTestBase::HoldingSpaceAshTestBase() = default;

HoldingSpaceAshTestBase::~HoldingSpaceAshTestBase() = default;

void HoldingSpaceAshTestBase::AddItem(HoldingSpaceItem::Type type,
                                      const base::FilePath& file_path) {
  auto* model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(model);
  model->AddItem(HoldingSpaceItem::CreateFileBackedItem(
      type, file_path,
      GURL(base::StrCat({"filesystem: ", file_path.BaseName().value()})),
      base::BindOnce(
          [](HoldingSpaceItem::Type type, const base::FilePath& file_path) {
            return std::make_unique<HoldingSpaceImage>(
                holding_space_util::GetMaxImageSizeForType(type), file_path,
                /*async_bitmap_resolver=*/base::DoNothing());
          })));
}

void HoldingSpaceAshTestBase::RemoveAllItems() {
  auto* model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(model);
  model->RemoveIf(
      base::BindRepeating([](const HoldingSpaceItem* item) { return true; }));
}

void HoldingSpaceAshTestBase::SetUp() {
  AshTestBase::SetUp();

  // Add and activate a new user.
  constexpr char kUserEmail[] = "user@test";
  AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  GetSessionControllerClient()->AddUserSession(kUserEmail);
  GetSessionControllerClient()->SwitchActiveUser(account_id);

  // Mark the holding space feature as being available to the user.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      GetSessionControllerClient()->GetUserPrefService(account_id));

  // Register a client and model that would normally be provided by the service.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &client_, &model_);
}

}  // namespace ash
