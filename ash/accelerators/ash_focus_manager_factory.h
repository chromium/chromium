// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ASH_FOCUS_MANAGER_FACTORY_H_
#define ASH_ACCELERATORS_ASH_FOCUS_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "ui/views/focus/focus_manager_factory.h"

namespace ash {

// A factory class for creating a custom views::FocusManager object which
// supports Ash shortcuts.
class AshFocusManagerFactory : public views::FocusManagerFactory {
 public:
  AshFocusManagerFactory();
  ~AshFocusManagerFactory() override;

 protected:
  // views::FocusManagerFactory overrides:
  std::unique_ptr<views::FocusManager> CreateFocusManager(
      views::Widget* widget,
      bool desktop_widget) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AshFocusManagerFactory);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ASH_FOCUS_MANAGER_FACTORY_H_
