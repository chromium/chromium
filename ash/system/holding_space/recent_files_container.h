// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_

#include "ui/views/view.h"

namespace ash {

// Container for the recent files (Screenshots, downloads etc).
class RecentFilesContainer : public views::View {
 public:
  RecentFilesContainer();
  RecentFilesContainer(const RecentFilesContainer& other) = delete;
  RecentFilesContainer& operator=(const RecentFilesContainer& other) = delete;
  ~RecentFilesContainer() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  views::View* screenshots_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_RECENT_FILES_CONTAINER_H_
