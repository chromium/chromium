// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_PAGE_TRANSITION_UTIL_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_PAGE_TRANSITION_UTIL_H_

#include "base/macros.h"
#include "ui/base/page_transition_types.h"

namespace apps {

// Returns true if ARC should ignore the navigation with the |page_transition|.
bool ShouldIgnoreNavigation(ui::PageTransition page_transition,
                            bool allow_form_submit,
                            bool allow_client_redirect);

// Removes |mask| bits from |page_transition|.
ui::PageTransition MaskOutPageTransition(ui::PageTransition page_transition,
                                         ui::PageTransition mask);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_PAGE_TRANSITION_UTIL_H_
