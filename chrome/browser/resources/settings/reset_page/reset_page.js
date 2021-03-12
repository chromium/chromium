// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 */
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import './reset_profile_dialog.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_shared_css.js';

// <if expr="_google_chrome and is_win">
import '../chrome_cleanup_page/chrome_cleanup_page.js';
import '../incompatible_applications_page/incompatible_applications_page.js';
// </if>

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';


Polymer({
  is: 'settings-reset-page',

  _template: html`{__html_template__}`,

  behaviors: [RouteObserverBehavior],

  properties: {
    /** Preferences state. */
    prefs: Object,

    // <if expr="_google_chrome and is_win">
    /** @private */
    showIncompatibleApplications_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('showIncompatibleApplications');
      },
    },
    // </if>
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @protected
   */
  currentRouteChanged(route) {
    const lazyRender =
        /** @type {!CrLazyRenderElement} */ (this.$.resetProfileDialog);

    if (route === routes.TRIGGERED_RESET_DIALOG ||
        route === routes.RESET_DIALOG) {
      /** @type {!SettingsResetProfileDialogElement} */ (lazyRender.get())
          .show();
    } else {
      const dialog = /** @type {?SettingsResetProfileDialogElement} */ (
          lazyRender.getIfExists());
      if (dialog) {
        dialog.cancel();
      }
    }
  },

  /** @private */
  onShowResetProfileDialog_() {
    Router.getInstance().navigateTo(
        routes.RESET_DIALOG, new URLSearchParams('origin=userclick'));
  },

  /** @private */
  onResetProfileDialogClose_() {
    Router.getInstance().navigateToPreviousRoute();
    focusWithoutInk(assert(this.$.resetProfile));
  },

  // <if expr="_google_chrome and is_win">
  /** @private */
  onChromeCleanupTap_() {
    Router.getInstance().navigateTo(routes.CHROME_CLEANUP);
  },

  /** @private */
  onIncompatibleApplicationsTap_() {
    Router.getInstance().navigateTo(routes.INCOMPATIBLE_APPLICATIONS);
  },
  // </if>
});
