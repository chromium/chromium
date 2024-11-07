// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_coral_delegate.h"

namespace ash {

TestCoralDelegate::TestCoralDelegate() = default;

TestCoralDelegate::~TestCoralDelegate() = default;

void TestCoralDelegate::LaunchPostLoginGroup(coral::mojom::GroupPtr group) {}

void TestCoralDelegate::MoveTabsInGroupToNewDesk(
    const std::vector<coral::mojom::Tab>& tabs) {}

void TestCoralDelegate::CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) {
}

}  // namespace ash
