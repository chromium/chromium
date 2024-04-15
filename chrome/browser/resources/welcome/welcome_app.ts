// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './google_apps/nux_google_apps.js';
import './landing_view.js';
import './ntp_background/nux_ntp_background.js';
import './set_as_default/nux_set_as_default.js';
import './signin_view.js';
import '../strings.m.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {NuxGoogleAppsElement} from './google_apps/nux_google_apps.js';
import {NavigationMixin} from './navigation_mixin.js';
import type {NuxNtpBackgroundElement} from './ntp_background/nux_ntp_background.js';
import {Routes} from './router.js';
import type {NuxSetAsDefaultElement} from './set_as_default/nux_set_as_default.js';
import {NuxSetAsDefaultProxyImpl} from './set_as_default/nux_set_as_default_proxy.js';
import {BookmarkBarManager} from './shared/bookmark_proxy.js';
import {getCss} from './welcome_app.css.js';
import {getHtml} from './welcome_app.html.js';
import {WelcomeBrowserProxyImpl} from './welcome_browser_proxy.js';

/**
 * The strings contained in the arrays should be valid DOM-element tag names.
 */
interface NuxOnboardingModules {
  'new-user': string[];
  'returning-user': string[];
}

/**
 * This list needs to be updated if new modules need to be supported in the
 * onboarding flow.
 */
const MODULES_WHITELIST: Set<string> = new Set([
  'nux-google-apps',
  'nux-ntp-background',
  'nux-set-as-default',
  'signin-view',
]);

/**
 * This list needs to be updated if new modules that need step-indicators are
 * added.
 */
const MODULES_NEEDING_INDICATOR: Set<string> =
    new Set(['nux-google-apps', 'nux-ntp-background', 'nux-set-as-default']);

export interface WelcomeAppElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const WelcomeAppElementBase = NavigationMixin(CrLitElement);

/** @polymer */
export class WelcomeAppElement extends WelcomeAppElementBase {
  static get is() {
    return 'welcome-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      modulesInitialized_: {type: Boolean},
    };
  }
  private currentRoute_: Routes|null = null;
  private modules_: NuxOnboardingModules;

  // Default to false so view-manager is hidden until views are
  // initialized.
  protected modulesInitialized_: boolean = false;

  constructor() {
    super();
    this.modules_ = {
      'new-user': loadTimeData.getString('newUserModules').split(','),
      'returning-user':
          loadTimeData.getString('returningUserModules').split(','),
    };
  }

  override firstUpdated() {
    this.setAttribute('role', 'main');
    this.addEventListener(
        'default-browser-change', () => this.onDefaultBrowserChange_());

    // Initiate focus-outline-manager for this document so that action-link
    // style can take advantage of it.
    FocusOutlineManager.forDocument(document);
  }

  private onDefaultBrowserChange_() {
    this.shadowRoot!.querySelector('cr-toast')!.show();
  }

  override onRouteChange(route: Routes, step: number) {
    const setStep = () => {
      // If the specified step doesn't exist, that means there are no more
      // steps. In that case, replace this page with NTP.
      if (!this.shadowRoot!.querySelector(`#step-${step}`)) {
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
    if (this.currentRoute_ !== route) {
      this.initializeModules(route).then(setStep);
    } else {
      setStep();
    }

    this.currentRoute_ = route;
  }

  initializeModules(route: Routes) {
    // Remove all views except landing.
    this.$.viewManager
        .querySelectorAll('[slot="view"]:not([id="step-landing"])')
        .forEach(element => element.remove());

    // If it is on landing route, end here.
    if (route === Routes.LANDING) {
      return Promise.resolve();
    }

    let modules = this.modules_[route];
    assert(modules);  // Modules should be defined if on a valid route.
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
            if (module === 'nux-set-as-default') {
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
            const element = document.createElement(elementTagName) as (
                                NuxGoogleAppsElement | NuxNtpBackgroundElement |
                                NuxSetAsDefaultElement);
            element.id = 'step-' + (index + 1);
            element.setAttribute('slot', 'view');
            if (MODULES_NEEDING_INDICATOR.has(elementTagName)) {
              element.indicatorModel = {
                total: indicatorElementCount,
                active: indicatorActiveCount++,
              };
            }
            this.$.viewManager.appendChild(element);
          });
        });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'welcome-app': WelcomeAppElement;
  }
}

customElements.define(WelcomeAppElement.is, WelcomeAppElement);
