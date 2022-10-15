// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/page_transition_util.h"

#include "base/check_op.h"
#include "base/types/cxx23_to_underlying.h"

namespace apps {

bool ShouldIgnoreNavigation(ui::PageTransition page_transition,
                            bool allow_form_submit,
                            bool is_in_fenced_frame_tree,
                            bool has_user_gesture) {
  // Navigations inside fenced frame trees are marked with
  // PAGE_TRANSITION_AUTO_SUBFRAME in order not to add session history items
  // (see https://crrev.com/c/3265344). So we only check |has_user_gesture|.
  if (is_in_fenced_frame_tree) {
    DCHECK(ui::PageTransitionCoreTypeIs(page_transition,
                                        ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    return !has_user_gesture;
  }

  // Mask out any redirect qualifiers
  page_transition = MaskOutPageTransition(page_transition,
                                          ui::PAGE_TRANSITION_IS_REDIRECT_MASK);

  if (!ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_LINK) &&
      !(allow_form_submit &&
        ui::PageTransitionCoreTypeIs(page_transition,
                                     ui::PAGE_TRANSITION_FORM_SUBMIT))) {
    // Do not handle the |url| if this event wasn't spawned by the user clicking
    // on a link.
    return true;
  }

  if (base::to_underlying(ui::PageTransitionGetQualifier(page_transition)) !=
      0) {
    // Qualifiers indicate that this navigation was the result of a click on a
    // forward/back button, or typing in the URL bar. Don't handle any of those
    // types of navigations.
    return true;
  }

  return false;
}

ui::PageTransition MaskOutPageTransition(ui::PageTransition page_transition,
                                         ui::PageTransition mask) {
  return ui::PageTransitionFromInt(page_transition & ~mask);
}

}  // namespace apps
