// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/views/help_bubble_factory_views_ash.h"

#include "ash/user_education/user_education_class_properties.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/views/help_bubble_view_ash.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace ash {

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleFactoryViewsAsh)

HelpBubbleFactoryViewsAsh::HelpBubbleFactoryViewsAsh(
    const user_education::HelpBubbleDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

HelpBubbleFactoryViewsAsh::~HelpBubbleFactoryViewsAsh() = default;

std::unique_ptr<user_education::HelpBubble>
HelpBubbleFactoryViewsAsh::CreateBubble(
    ui::TrackedElement* element,
    user_education::HelpBubbleParams params) {
  internal::HelpBubbleAnchorParams anchor;
  anchor.view = element->AsA<views::TrackedElementViews>()->view();
  return CreateBubbleImpl(element, anchor, std::move(params));
}

bool HelpBubbleFactoryViewsAsh::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<views::TrackedElementViews>() &&
         element->AsA<views::TrackedElementViews>()->view()->GetProperty(
             kHelpBubbleContextKey) == HelpBubbleContext::kAsh;
}

std::unique_ptr<user_education::HelpBubble>
HelpBubbleFactoryViewsAsh::CreateBubbleImpl(
    ui::TrackedElement* element,
    const internal::HelpBubbleAnchorParams& anchor,
    user_education::HelpBubbleParams params) {
  anchor.view->SetProperty(user_education::kHasInProductHelpPromoKey, true);

  // NOTE: `HelpBubbleViewAsh` instances are owned by their widgets.
  const HelpBubbleId help_bubble_id =
      user_education_util::GetHelpBubbleId(params.extended_properties);
  auto result = base::WrapUnique(new HelpBubbleViewsAsh(
      HelpBubbleViewAsh::Create(help_bubble_id, anchor, std::move(params)),
      element));

  for (const auto& accelerator :
       delegate_->GetPaneNavigationAccelerators(element)) {
    result->help_bubble_view_->GetFocusManager()->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::HandlerPriority::kNormalPriority,
        result.get());
  }

  return result;
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleViewsAsh)

HelpBubbleViewsAsh::HelpBubbleViewsAsh(user_education::HelpBubbleViewInfo info,
                                       ui::TrackedElement* anchor_element)
    : HelpBubbleViews(std::move(info), anchor_element) {}

HelpBubbleViewsAsh::~HelpBubbleViewsAsh() = default;

}  // namespace ash
