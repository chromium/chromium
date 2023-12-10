// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_TEST_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_
#define ASH_SYSTEM_HOLDING_SPACE_TEST_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_

#include <memory>
#include <vector>

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Simple implementation of the abstract class `HoldingSpaceTrayChildBubble` for
// testing. Allows callers to pass in callbacks through `Params` to dictate what
// sections and placeholder are created.
class TestHoldingSpaceTrayChildBubble : public HoldingSpaceTrayChildBubble {
  METADATA_HEADER(TestHoldingSpaceTrayChildBubble, HoldingSpaceTrayChildBubble)

 public:
  struct Params {
    using CreateSectionsCallback = base::OnceCallback<
        std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>(
            HoldingSpaceViewDelegate* view_delegate)>;
    using CreatePlaceholderCallback =
        base::OnceCallback<std::unique_ptr<views::View>()>;

    Params();
    Params(Params&& other);
    explicit Params(CreateSectionsCallback create_sections_callback,
                    CreatePlaceholderCallback create_placeholder_callback =
                        base::NullCallback());
    ~Params();

    CreateSectionsCallback create_sections_callback;
    CreatePlaceholderCallback create_placeholder_callback;
  };

  TestHoldingSpaceTrayChildBubble(HoldingSpaceViewDelegate* view_delegate,
                                  Params params);

 private:
  // HoldingSpaceChildBubble:
  std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>> CreateSections()
      override;
  std::unique_ptr<views::View> CreatePlaceholder() override;

  Params params_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_TEST_HOLDING_SPACE_TRAY_CHILD_BUBBLE_H_
