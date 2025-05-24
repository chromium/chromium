// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_NON_ACCESSIBLE_VIEW_H_
#define ASH_LOGIN_UI_NON_ACCESSIBLE_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// A views::View instance that is not shown in the accessibility hierarchy.
class ASH_EXPORT NonAccessibleView : public views::View {
  METADATA_HEADER(NonAccessibleView, views::View)

 public:
  NonAccessibleView();
  explicit NonAccessibleView(const std::string& name);

  NonAccessibleView(const NonAccessibleView&) = delete;
  NonAccessibleView& operator=(const NonAccessibleView&) = delete;

  ~NonAccessibleView() override;

  // views::View:
  std::string GetObjectName() const override;

 private:
  const std::string name_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_NON_ACCESSIBLE_VIEW_H_
