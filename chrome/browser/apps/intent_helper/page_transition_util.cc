// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/page_transition_util.h"

namespace apps {

bool ShouldIgnoreNavigation(ui::PageTransition page_transition,
                            bool allow_form_submit,
                            bool allow_client_redirect) {
  // |allow_client_redirect| is true only for non-http(s) cases, and for those
  // we can ignore the CLIENT/SERVER REDIRECT flags, otherwise mask out the
  // SERVER_REDIRECT flag only.
  page_transition = MaskOutPageTransition(
      page_transition, allow_client_redirect
                           ? ui::PAGE_TRANSITION_IS_REDIRECT_MASK
                           : ui::PAGE_TRANSITION_SERVER_REDIRECT);

  if (!ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_LINK) &&
      !(allow_form_submit &&
        ui::PageTransitionCoreTypeIs(page_transition,
                                     ui::PAGE_TRANSITION_FORM_SUBMIT))) {
    // Do not handle the |url| if this event wasn't spawned by the user clicking
    // on a link.
    return true;
  }

  if (ui::PageTransitionGetQualifier(page_transition) != 0) {
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
