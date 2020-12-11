// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_

#include <memory>
#include <vector>

#include "ui/views/view.h"

namespace ash {

class HoldingSpaceItemViewDelegate;
class HoldingSpaceItemViewsSection;

// Child bubble of the `HoldingSpaceTrayBubble`.
class HoldingSpaceTrayChildBubble : public views::View {
 public:
  explicit HoldingSpaceTrayChildBubble(HoldingSpaceItemViewDelegate* delegate);
  HoldingSpaceTrayChildBubble(const HoldingSpaceTrayChildBubble& other) =
      delete;
  HoldingSpaceTrayChildBubble& operator=(
      const HoldingSpaceTrayChildBubble& other) = delete;
  ~HoldingSpaceTrayChildBubble() override;

  // Initializes the child bubble.
  void Init();

  // Resets the child bubble. Called when the tray bubble starts closing to stop
  // observing the holding space controller/model to ensure that no new items
  // are created while the bubble widget is begin asynchronously closed.
  void Reset();

 protected:
  // Invoked to create the `sections_` for this child bubble.
  virtual std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>
  CreateSections() = 0;

  HoldingSpaceItemViewDelegate* delegate() { return delegate_; }

 private:
  // views::View:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  HoldingSpaceItemViewDelegate* const delegate_;

  // Views owned by view hierarchy.
  std::vector<HoldingSpaceItemViewsSection*> sections_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_
