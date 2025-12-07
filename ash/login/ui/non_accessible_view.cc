// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/non_accessible_view.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {
constexpr const char kDefaultName[] = "NonAccessibleView";
}  // namespace

NonAccessibleView::NonAccessibleView() : NonAccessibleView(kDefaultName) {}

NonAccessibleView::NonAccessibleView(const std::string& name) : name_(name) {
  GetViewAccessibility().SetIsInvisible(true);
}

NonAccessibleView::~NonAccessibleView() = default;

std::string NonAccessibleView::GetObjectName() const {
  return name_;
}

BEGIN_METADATA(NonAccessibleView)
END_METADATA

}  // namespace ash