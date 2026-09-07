// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CORAL_CORAL_TEST_UTIL_H_
#define ASH_WM_CORAL_CORAL_TEST_UTIL_H_

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ash/birch/coral_constants.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"

class AccountId;

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace ash {
class CoralChipButton;
class TabAppSelectionHost;

// Installs a process-wide IdentityManagerProvider test double whose primary
// account reports the ChromeOS GenAI capability. This makes Coral's GenAI age
// check resolve to "available" in tests, matching the behavior of the previous
// TestCoralDelegate stub. Construct one in a test's SetUp() before entering
// overview so the coral data fetch does not crash on the missing provider.
class ScopedCoralGenAIAvailability {
 public:
  explicit ScopedCoralGenAIAvailability(const AccountId& account_id);
  ScopedCoralGenAIAvailability(const ScopedCoralGenAIAvailability&) = delete;
  ScopedCoralGenAIAvailability& operator=(const ScopedCoralGenAIAvailability&) =
      delete;
  ~ScopedCoralGenAIAvailability();

 private:
  // Holds the IdentityManager/UserManager/SessionManager test doubles. Hidden
  // in the .cc so this public header does not pull in those dependencies.
  class Impl;
  std::unique_ptr<Impl> impl_;
};

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
    const std::optional<std::string>& title = std::nullopt,
    const base::Token& id = base::Token());

// Creates a group with some default urls and apps.
coral::mojom::GroupPtr CreateDefaultTestGroup();

void OverrideTestResponse(std::vector<coral::mojom::GroupPtr> test_groups,
                          CoralSource source = CoralSource::kUnknown);

// Brings up the selector menu host object by entering overview and clicking
// the birch coral chip.
TabAppSelectionHost* ShowAndGetSelectorMenu(
    ui::test::EventGenerator* event_generator);

// Gets the first coral button on the primary root window.
CoralChipButton* GetFirstCoralButton();

// Gets the number of coral chips on the primary root window.
size_t GetCoralButtonNum();

}  // namespace ash

#endif  // ASH_WM_CORAL_CORAL_TEST_UTIL_H_
