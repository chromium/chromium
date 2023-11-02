// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ASH_FOCUS_MANAGER_FACTORY_H_
#define ASH_ACCELERATORS_ASH_FOCUS_MANAGER_FACTORY_H_

#include "ui/views/focus/focus_manager_factory.h"

namespace ash {

// A factory class for creating a custom views::FocusManager object which
// supports Ash shortcuts.
class AshFocusManagerFactory : public views::FocusManagerFactory {
 public:
  AshFocusManagerFactory();
  AshFocusManagerFactory(const AshFocusManagerFactory&) = delete;
  AshFocusManagerFactory& operator=(const AshFocusManagerFactory&) = delete;
  ~AshFocusManagerFactory() override;

 protected:
  // views::FocusManagerFactory overrides:
  std::unique_ptr<views::FocusManager> CreateFocusManager(
      views::Widget* widget) override;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ASH_FOCUS_MANAGER_FACTORY_H_
