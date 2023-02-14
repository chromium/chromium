// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {routes} from '../../os_settings_routes.js';
import {Router} from '../../router.js';

/**
 * Navigates to the App Detail page.
 */
export function openAppDetailPage(appId: string): void {
  const params = new URLSearchParams();
  params.append('id', appId);
  Router.getInstance().navigateTo(routes.APP_MANAGEMENT_DETAIL, params);
}

/**
 * Navigates to the main App Management list page.
 */
export function openMainPage(): void {
  Router.getInstance().navigateTo(routes.APP_MANAGEMENT);
}
