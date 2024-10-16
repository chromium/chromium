// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CORAL_CORAL_TEST_UTIL_H_
#define ASH_WM_CORAL_CORAL_TEST_UTIL_H_

#include <string>
#include <variant>
#include <vector>

#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace ash {
class TabAppSelectionHost;

// Test struct that holds a string and a GURL or additional string. Simplifies
// test code by allowing callsites to use initializer lists.
struct TestEntity {
  TestEntity(const std::string& title,
             const std::variant<GURL, std::string>& id);
  TestEntity(const TestEntity&);
  TestEntity& operator=(const TestEntity&);
  ~TestEntity();
  std::string title;
  std::variant<GURL, std::string> id;
};

// Creates a group for testing purposes. `entities` is a vector of GURLs or app
// ids.
coral::mojom::GroupPtr CreateTestGroup(
    const std::vector<TestEntity>& entities,
    const std::string& title = std::string());

// Creates a group with some default urls and apps.
coral::mojom::GroupPtr CreateDefaultTestGroup();

void OverrideTestResponse(std::vector<coral::mojom::GroupPtr> test_groups);

// Brings up the selector menu host object by entering overview and clicking
// the birch coral chip.
TabAppSelectionHost* ShowAndGetSelectorMenu(
    ui::test::EventGenerator* event_generator);

}  // namespace ash

#endif  // ASH_WM_CORAL_CORAL_TEST_UTIL_H_
