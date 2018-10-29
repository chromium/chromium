// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The strings contained in the arrays should be valid DOM-element tag names.
 * @typedef {{
 *   'new-user': !Array<string>,
 *   'returning-user': !Array<string>
 * }}
 */
let NuxOnboardingModules;

// This list needs to be updated if new modules that need step-indicators are
// added.
const MODELS_NEEDING_INDICATOR =
    ['nux-email', 'nux-google-apps', 'nux-set-as-default'];

Polymer({
  is: 'welcome-app',

  behaviors: [welcome.NavigationBehavior],

  /** @private {?welcome.Routes} */
  currentRoute_: null,

  /** @private {!PromiseResolver} */
  defaultCheckPromise_: new PromiseResolver(),

  // TODO(scottchen): instead of dummy, get data from finch/load time data.
  /** @private {NuxOnboardingModules} */
  modules_: {
    'new-user':
        ['nux-email', 'nux-google-apps', 'nux-set-as-default', 'signin-view'],
    'returning-user': ['nux-set-as-default'],
  },

  /** @override */
  ready: function() {
    /** @param {!nux.DefaultBrowserInfo} status */
    const defaultCheckCallback = status => {
      if (status.isDefault || !status.canBeDefault) {
        this.defaultCheckPromise_.resolve(false);
      } else if (!status.isDisabledByPolicy && !status.isUnknownError) {
        this.defaultCheckPromise_.resolve(true);
      } else {  // Unknown error.
        this.defaultCheckPromise_.resolve(false);
      }

      cr.removeWebUIListener(defaultCheckCallback);
    };

    cr.addWebUIListener('browser-default-state-changed', defaultCheckCallback);

    // TODO(scottchen): convert the request to cr.sendWithPromise
    // (see https://crbug.com/874520#c6).
    nux.NuxSetAsDefaultProxyImpl.getInstance().requestDefaultBrowserState();
  },

  /**
   * @param {welcome.Routes} route
   * @param {number} step
   * @private
   */
  onRouteChange: function(route, step) {
    const setStep = () => {
      // If the specified step doesn't exist, that means there are no more
      // steps. In that case, replace this page with NTP.
      if (!this.$$(`#step-${step}`)) {
        welcome.WelcomeBrowserProxyImpl.getInstance().goToNewTabPage(
            /* replace */ true);
      } else {  // Otherwise, go to the chosen step of that route.
        this.$.viewManager.switchView(`step-${step}`);
      }
    };

    // If the route changed, initialize the steps of modules for that route.
    if (this.currentRoute_ != route && route != welcome.Routes.LANDING) {
      this.initializeModules(this.modules_[route]).then(setStep);
    } else {
      setStep();
    }

    this.currentRoute_ = route;
  },

  /** @param {!Array<string>} modules Array of valid DOM element names. */
  initializeModules: function(modules) {
    assert(modules);  // modules should be defined if on a valid route.

    // Wait until the default-browser state is known before anything
    // initializes.
    return this.defaultCheckPromise_.promise.then(canSetDefault => {
      if (!canSetDefault)
        modules = modules.filter(module => module != 'nux-set-as-default');

      // Remove all views except landing.
      this.$.viewManager
          .querySelectorAll('[slot="view"]:not([id="step-landing"])')
          .forEach(element => {
            element.remove();
          });

      let indicatorElementCount = 0;
      for (let i = 0; i < modules.length; i++) {
        if (MODELS_NEEDING_INDICATOR.includes(modules[i]))
          indicatorElementCount++;
      }

      let indicatorActiveCount = 0;
      modules.forEach((elementTagName, index) => {
        const element = document.createElement(elementTagName);
        element.id = 'step-' + (index + 1);
        element.setAttribute('slot', 'view');
        this.$.viewManager.appendChild(element);

        if (MODELS_NEEDING_INDICATOR.includes(elementTagName)) {
          element.indicatorModel = {
            total: indicatorElementCount,
            active: indicatorActiveCount++,
          };
        }
      });
    });
  },
});
