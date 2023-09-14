// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_ash_test_base.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/strings/strcat.h"

namespace ash {
namespace {

std::unique_ptr<HoldingSpaceImage> CreateStubHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      holding_space_util::GetMaxImageSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

}  // namespace

HoldingSpaceAshTestBase::HoldingSpaceAshTestBase() = default;

HoldingSpaceAshTestBase::~HoldingSpaceAshTestBase() = default;

HoldingSpaceItem* HoldingSpaceAshTestBase::AddItem(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  std::unique_ptr<HoldingSpaceItem> item =
      HoldingSpaceItem::CreateFileBackedItem(
          type,
          HoldingSpaceFile(file_path, HoldingSpaceFile::FileSystemType::kTest,
                           GURL(base::StrCat(
                               {"filesystem:", file_path.BaseName().value()}))),
          base::BindOnce(&CreateStubHoldingSpaceImage));
  auto* item_ptr = item.get();
  DCHECK(model());
  model()->AddItem(std::move(item));
  return item_ptr;
}

HoldingSpaceItem* HoldingSpaceAshTestBase::AddPartiallyInitializedItem(
    HoldingSpaceItem::Type type,
    const base::FilePath& path) {
  // Create a holding space item, and use it to create a serialized item
  // dictionary, then immediately deserialize it. This results in a
  // partially initialized item, since it does not have a `GURL` for the
  // backing file.
  std::unique_ptr<HoldingSpaceItem> item =
      HoldingSpaceItem::CreateFileBackedItem(
          type,
          HoldingSpaceFile(path, HoldingSpaceFile::FileSystemType::kTest,
                           GURL("filesystem:ignored")),
          base::BindOnce(&CreateStubHoldingSpaceImage));
  const base::Value::Dict serialized_holding_space_item = item->Serialize();
  std::unique_ptr<HoldingSpaceItem> deserialized_item =
      HoldingSpaceItem::Deserialize(
          serialized_holding_space_item,
          /*image_resolver=*/
          base::BindOnce(&CreateStubHoldingSpaceImage));
  DCHECK(!deserialized_item->IsInitialized());

  HoldingSpaceItem* deserialized_item_ptr = deserialized_item.get();
  DCHECK(model());
  model()->AddItem(std::move(deserialized_item));
  return deserialized_item_ptr;
}

void HoldingSpaceAshTestBase::RemoveAllItems() {
  ASSERT_TRUE(model());
  model()->RemoveIf(
      base::BindRepeating([](const HoldingSpaceItem* item) { return true; }));
}

void HoldingSpaceAshTestBase::MarkTimeOfFirstAdd(AccountId account_id) {
  holding_space_prefs::MarkTimeOfFirstAdd(
      GetSessionControllerClient()->GetUserPrefService(account_id));
}

void HoldingSpaceAshTestBase::SetUp() {
  AshTestBase::SetUp();

  // Add and activate a new user.
  AccountId account_id = AccountId::FromUserEmail(kTestUser);
  GetSessionControllerClient()->AddUserSession(kTestUser);
  GetSessionControllerClient()->SwitchActiveUser(account_id);

  // Mark the holding space feature as being available to the user.
  holding_space_prefs::MarkTimeOfFirstAvailability(
      GetSessionControllerClient()->GetUserPrefService(account_id));

  // Register a client and model that would normally be provided by the service.
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &client_, &model_);
}

}  // namespace ash
