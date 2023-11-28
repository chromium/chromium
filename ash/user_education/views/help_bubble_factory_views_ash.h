// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_ASH_H_
#define ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_ASH_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_params.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace user_education {
class HelpBubbleDelegate;
}  // namespace user_education

namespace ash {

class HelpBubbleViewAsh;

namespace internal {
struct HelpBubbleAnchorParams;
}

// Views-specific implementation of the help bubble.
//
// Because this is a FrameworkSpecificImplementation, you can use:
//   help_bubble->AsA<HelpBubbleViewsAsh>()->bubble_view()
// to retrieve the underlying bubble view.
class ASH_EXPORT HelpBubbleViewsAsh : public user_education::HelpBubble,
                                      public views::WidgetObserver,
                                      public ui::AcceleratorTarget {
 public:
  ~HelpBubbleViewsAsh() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // Retrieve the bubble view. If the bubble has been closed, this may return
  // null.
  HelpBubbleViewAsh* bubble_view() { return help_bubble_view_; }
  const HelpBubbleViewAsh* bubble_view() const { return help_bubble_view_; }

  // HelpBubble:
  bool ToggleFocusForAccessibility() override;
  void OnAnchorBoundsChanged() override;
  gfx::Rect GetBoundsInScreen() const override;
  ui::ElementContext GetContext() const override;

  // ui::AcceleratorTarget
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  friend class HelpBubbleFactoryViewsAsh;
  friend class HelpBubbleFactoryMac;
  friend class HelpBubbleViewsTest;

  explicit HelpBubbleViewsAsh(HelpBubbleViewAsh* help_bubble_view,
                              ui::TrackedElement* anchor_element);

  // Clean up properties on the anchor view, if applicable.
  void MaybeResetAnchorView();

  // HelpBubble:
  void CloseBubbleImpl() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void OnElementHidden(ui::TrackedElement* element);
  void OnElementBoundsChanged(ui::TrackedElement* element);

  raw_ptr<HelpBubbleViewAsh> help_bubble_view_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_observation_{this};

  // Track the anchor element to determine if/when it goes away.
  raw_ptr<const ui::TrackedElement, DanglingUntriaged> anchor_element_;

  // Listens so that the bubble can be closed if the anchor element disappears.
  // The specific anchor view is not tracked because in a few cases (e.g. Mac
  // native menus) the anchor view is not the anchor element itself but a
  // placeholder.
  base::CallbackListSubscription anchor_hidden_subscription_;

  // Listens for changes to the anchor bounding rect that are independent of the
  // anchor view. Necessary for e.g. WebUI elements, which can be scrolled or
  // moved within the web page.
  base::CallbackListSubscription anchor_bounds_changed_subscription_;

  base::WeakPtrFactory<HelpBubbleViewsAsh> weak_ptr_factory_{this};
};

// Factory implementation for HelpBubbleViews.
class ASH_EXPORT HelpBubbleFactoryViewsAsh
    : public user_education::HelpBubbleFactory {
 public:
  explicit HelpBubbleFactoryViewsAsh(
      const user_education::HelpBubbleDelegate* delegate);
  ~HelpBubbleFactoryViewsAsh() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // user_education::HelpBubbleFactory:
  std::unique_ptr<user_education::HelpBubble> CreateBubble(
      ui::TrackedElement* element,
      user_education::HelpBubbleParams params) override;
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;

 protected:
  std::unique_ptr<user_education::HelpBubble> CreateBubbleImpl(
      ui::TrackedElement* element,
      const internal::HelpBubbleAnchorParams& anchor,
      user_education::HelpBubbleParams params);

 private:
  raw_ptr<const user_education::HelpBubbleDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_ASH_H_
