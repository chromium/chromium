// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/chrome_navigation_initiator.h"

#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "content/public/browser/navigation_handle.h"

void MarkNavigationServedBySearchPrefetch(
    content::NavigationHandle& navigation_handle) {
  // Note: `GetOrCreate`, not `Get`. No trigger creates this user data anymore,
  // as they attach `page_load_metrics::NavigationInitiatorHolder` instead.
  page_load_metrics::NavigationHandleUserData::GetOrCreateForNavigationHandle(
      navigation_handle)
      ->set_is_served_by_legacy_search_prefetch(true);
}
