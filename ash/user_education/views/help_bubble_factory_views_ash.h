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
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_factory.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
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

namespace internal {
struct HelpBubbleAnchorParams;
}

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
