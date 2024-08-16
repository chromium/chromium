// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_error_bubble.h"

#include "ash/login/ui/login_test_base.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

class AnchorView final : public views::View {
 public:
  base::WeakPtr<AnchorView> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  METADATA_HEADER(AnchorView, views::View)
  base::WeakPtrFactory<AnchorView> weak_ptr_factory_{this};
};

BEGIN_METADATA(AnchorView)
END_METADATA

}  // namespace

using LoginErrorBubbleTest = LoginTestBase;

TEST_F(LoginErrorBubbleTest, PersistentEventHandling) {
  auto* container = new views::View;
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(container));

  auto* anchor_view = new AnchorView();
  container->AddChildView(anchor_view);

  auto* bubble = new LoginErrorBubble(anchor_view->AsWeakPtr());
  bubble->set_persistent(true);
  container->AddChildView(bubble);

  EXPECT_FALSE(bubble->GetVisible());

  bubble->Show();
  EXPECT_TRUE(bubble->GetVisible());

  ui::test::EventGenerator* generator = GetEventGenerator();

  generator->MoveMouseTo(anchor_view->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_TRUE(bubble->GetVisible());

  generator->MoveMouseTo(bubble->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_TRUE(bubble->GetVisible());

  generator->GestureTapAt(anchor_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(bubble->GetVisible());

  generator->GestureTapAt(bubble->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(bubble->GetVisible());

  generator->PressKey(ui::KeyboardCode::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(bubble->GetVisible());
}

TEST_F(LoginErrorBubbleTest, AccessibleProperties) {
  auto* container = new views::View;
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetWidget(CreateWidgetWithContent(container));

  auto* anchor_view = new AnchorView();
  container->AddChildView(anchor_view);

  auto* bubble = new LoginErrorBubble(anchor_view->AsWeakPtr());
  container->AddChildView(bubble);

  ui::AXNodeData data;
  bubble->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kAlertDialog);
}

}  // namespace ash
