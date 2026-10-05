// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_SHELF_ITEM_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_SHELF_ITEM_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/types/expected.h"
#include "ui/aura/window_observer.h"

namespace ash {

// A test version of ShelfItemDelegate that supports optional window tracking.
class TestShelfItemDelegate : public ShelfItemDelegate,
                              public aura::WindowObserver {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id);
  ~TestShelfItemDelegate() override;

  void AddWindow(aura::Window* window);

  // ShelfItemDelegate:
  AppMenuItems GetAppMenuItems(
      int event_flags,
      const ItemFilterPredicate& filter_predicate) override;
  base::expected<aura::Window*, std::u16string> GetAppMenuItemWindow(
      int command_id) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  std::vector<raw_ptr<aura::Window>> windows_;
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_SHELF_ITEM_DELEGATE_H_
