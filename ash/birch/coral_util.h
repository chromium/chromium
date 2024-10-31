// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_CORAL_UTIL_H_
#define ASH_BIRCH_CORAL_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "base/values.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"

namespace aura {
class Window;
}

namespace ash::coral_util {

struct ASH_EXPORT TabsAndApps {
  TabsAndApps();
  TabsAndApps(const TabsAndApps& other);
  TabsAndApps& operator=(const TabsAndApps& other);
  ~TabsAndApps();

  std::vector<coral::mojom::Tab> tabs;
  std::vector<coral::mojom::App> apps;
};

std::string GetIdentifier(const coral::mojom::EntityPtr& item);

std::string GetIdentifier(const coral::mojom::Entity& item);

// Checks if the given `window` (or its tabs if it is a browser window) can be
// moved to a new desk.
bool CanMoveToNewDesk(aura::Window* window);

// Splits an entity pointer vector `content` into its tab and app components.
// This is so we can use EXPECT_THAT in tests.
TabsAndApps ASH_EXPORT
SplitContentData(const std::vector<coral::mojom::EntityPtr>& content);

// For debugging logs.
base::Value::List EntitiesToListValue(
    const std::vector<coral::mojom::EntityPtr>& entities);
std::string GroupToString(const coral::mojom::GroupPtr& group);

}  // namespace ash::coral_util

#endif  // ASH_BIRCH_CORAL_UTIL_H_
