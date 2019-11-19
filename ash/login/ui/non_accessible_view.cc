// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/non_accessible_view.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace ash {

namespace {
constexpr const char kDefaultName[] = "NonAccessibleView";
}  // namespace

NonAccessibleView::NonAccessibleView() : NonAccessibleView(kDefaultName) {}

NonAccessibleView::NonAccessibleView(const std::string& name) : name_(name) {}

NonAccessibleView::~NonAccessibleView() = default;

const char* NonAccessibleView::GetClassName() const {
  return name_.c_str();
}

void NonAccessibleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->AddState(ax::mojom::State::kInvisible);
}

}  // namespace ash