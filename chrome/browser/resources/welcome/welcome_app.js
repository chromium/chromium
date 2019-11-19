// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './google_apps/nux_google_apps.js';
import './landing_view.js';
import './ntp_background/nux_ntp_background.js';
import './set_as_default/nux_set_as_default.js';
import './signin_view.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigateTo, navigateToNextStep, NavigationBehavior, Routes} from './navigation_behavior.js';
import {NuxSetAsDefaultProxyImpl} from './set_as_default/nux_set_as_default_proxy.js';
import {BookmarkBarManager} from './shared/bookmark_proxy.js';
import {WelcomeBrowserProxyImpl} from './welcome_browser_proxy.js';

/**
 * The strings contained in the arrays should be valid DOM-element tag names.
 * @typedef {{
 *   'new-user': !Array<string>,
 *   'returning-user': !Array<string>
 * }}
 */
let NuxOnboardingModules;

/**
 * This list needs to be updated if new modules need to be supported in the
 * onboarding flow.
 * @const {!Set<string>}
 */
const MODULES_WHITELIST = new Set([
  'nux-google-apps', 'nux-ntp-background', 'nux-set-as-default', 'signin-view'
]);

/**
 * This list needs to be updated if new modules that need step-indicators are
 * added.
 * @const {!Set<string>}
 */
const MODULES_NEEDING_INDICATOR =
    new Set(['nux-google-apps', 'nux-ntp-background', 'nux-set-as-default']);

Polymer({
  is: 'welcome-app',

  _template: html`{__html_template__}`,

  behaviors: [NavigationBehavior],

  /** @private {?Routes} */
  currentRoute_: null,

  /** @private {NuxOnboardingModules} */
  modules_: {
    'new-user': loadTimeData.getString('newUserModules').split(','),
    'returning-user': loadTimeData.getString('returningUserModules').split(','),
  },

  properties: {
    /** @private */
    modulesInitialized_: {
      type: Boolean,
      // Default to false so view-manager is hidden until views are initialized.
      value: false,
    },
  },

  hostAttributes: {
    role: 'main',
  },

  listeners: {
    'default-browser-change': 'onDefaultBrowserChange_',
  },

  /** @private */
  onDefaultBrowserChange_: function() {
    this.$$('cr-toast').show();
  },

  /**
   * @param {Routes} route
   * @param {number} step
   * @private
   */
  onRouteChange: function(route, step) {
    const setStep = () => {
      // If the specified step doesn't exist, that means there are no more
      // steps. In that case, replace this page with NTP.
      if (!this.$$(`#step-${step}`)) {
        WelcomeBrowserProxyImpl.getInstance().goToNewTabPage(
            /* replace */ true);
      } else {  // Otherwise, go to the chosen step of that route.
        // At this point, views are ready to be shown.
        this.modulesInitialized_ = true;
        this.$.viewManager.switchView(
            `step-${step}`, 'fade-in', 'no-animation');
      }
    };

    // If the route changed, initialize the steps of modules for that route.
    if (this.currentRoute_ != route) {
      this.initializeModules(route).then(setStep);
    } else {
      setStep();
    }

    this.currentRoute_ = route;
  },

  /** @param {Routes} route */
  initializeModules: function(route) {
    // Remove all views except landing.
    this.$.viewManager
        .querySelectorAll('[slot="view"]:not([id="step-landing"])')
        .forEach(element => element.remove());

    // If it is on landing route, end here.
    if (route == Routes.LANDING) {
      return Promise.resolve();
    }

    let modules = this.modules_[route];
    assert(modules);  // Modules should be defined if on a valid route.

    /** @type {!Promise} */
    const defaultBrowserPromise =
        NuxSetAsDefaultProxyImpl.getInstance()
            .requestDefaultBrowserState()
            .then((status) => {
              if (status.isDefault || !status.canBeDefault) {
                return false;
              } else if (!status.isDisabledByPolicy && !status.isUnknownError) {
                return true;
              } else {  // Unknown error.
                return false;
              }
            });

    // Wait until the default-browser state and bookmark visibility are known
    // before anything initializes.
    return Promise
        .all([
          defaultBrowserPromise,
          BookmarkBarManager.getInstance().initialized,
        ])
        .then(([canSetDefault]) => {
          modules = modules.filter(module => {
            if (module == 'nux-set-as-default') {
              return canSetDefault;
            }

            if (!MODULES_WHITELIST.has(module)) {
              // Makes sure the module specified by the feature configuration is
              // whitelisted.
              return false;
            }

            return true;
          });

          const indicatorElementCount = modules.reduce((count, module) => {
            return count += MODULES_NEEDING_INDICATOR.has(module) ? 1 : 0;
          }, 0);

          let indicatorActiveCount = 0;
          modules.forEach((elementTagName, index) => {
            const element = document.createElement(elementTagName);
            element.id = 'step-' + (index + 1);
            element.setAttribute('slot', 'view');
            this.$.viewManager.appendChild(element);

            if (MODULES_NEEDING_INDICATOR.has(elementTagName)) {
              element.indicatorModel = {
                total: indicatorElementCount,
                active: indicatorActiveCount++,
              };
            }
          });
        });
  },
});
