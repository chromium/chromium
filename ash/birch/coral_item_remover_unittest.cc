// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/coral_item_remover.h"

#include "ash/public/cpp/coral_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {
using ::testing::ElementsAre;
using ::testing::Eq;
}  // namespace

using CoralItemRemoverTest = ::testing::Test;

TEST_F(CoralItemRemoverTest, FilterContent) {
  CoralItemRemover coral_item_remover_;
  auto item0 = coral::mojom::Entity::NewTab(
      coral::mojom::Tab::New("tab 0 title", GURL("http://tab0.com")));
  auto item1 = coral::mojom::Entity::NewTab(
      coral::mojom::Tab::New("tab 1 title", GURL("http://tab1.com")));
  auto item2 = coral::mojom::Entity::NewApp(
      coral::mojom::App::New("app 0 name", "app 0 id"));
  auto item3 = coral::mojom::Entity::NewApp(
      coral::mojom::App::New("app 1 name", "app 1 id"));
  std::vector<coral::mojom::EntityPtr> entities;
  entities.push_back(item0.Clone());
  entities.push_back(item1.Clone());
  entities.push_back(item2.Clone());
  entities.push_back(item3.Clone());

  // Filter `tab_items` before any items are removed. The list should remain
  // unchanged.
  coral_item_remover_.FilterRemovedItems(&entities);
  ASSERT_EQ(4u, entities.size());

  // Remove `item2`, and filter it from the list of tabs.
  coral_item_remover_.RemoveItem(*item2);
  coral_item_remover_.FilterRemovedItems(&entities);

  // Check that `item2` is filtered out.
  ASSERT_EQ(3u, entities.size());
  EXPECT_THAT(entities, ElementsAre(Eq(std::ref(item0)), Eq(std::ref(item1)),
                                    Eq(std::ref(item3))));

  // Remove `item1`, and filter it from the list of tabs.
  coral_item_remover_.RemoveItem(*item1);
  coral_item_remover_.FilterRemovedItems(&entities);

  // Check that `item1` is filtered out.
  ASSERT_EQ(2u, entities.size());
  EXPECT_THAT(entities, ElementsAre(Eq(std::ref(item0)), Eq(std::ref(item3))));
}

}  // namespace ash
