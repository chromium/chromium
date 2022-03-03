// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-reset-page' is the OS settings page containing reset
 * settings.
 */
import './os_powerwash_dialog.js';

import {getEuicc, getNonPendingESimProfiles} from '//resources/cr_components/chromeos/cellular_setup/esim_manager_utils.m.js';
import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-reset-page',

  behaviors: [DeepLinkingBehavior, RouteObserverBehavior],

  properties: {
    /** @private */
    showPowerwashDialog_: Boolean,

    /**
     * @type {!Array<!ash.cellularSetup.mojom.ESimProfileRemote>}
     * @private
     */
    installedESimProfiles_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([chromeos.settings.mojom.Setting.kPowerwash]),
    },
  },

  /** @private */
  /**
   * @param {!Event} e
   * @private
   */
  onShowPowerwashDialog_(e) {
    e.preventDefault();

    getEuicc().then(euicc => {
      if (!euicc) {
        this.installedESimProfiles_ = [];
        this.showPowerwashDialog_ = true;
        return;
      }
      getNonPendingESimProfiles(euicc).then(profiles => {
        this.installedESimProfiles_ = profiles;
        this.showPowerwashDialog_ = true;
      });
    });
  },

  /** @private */
  onPowerwashDialogClose_() {
    this.showPowerwashDialog_ = false;
    focusWithoutInk(assert(this.$.powerwash));
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} newRoute
   * @param {!Route} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    // Does not apply to this page.
    if (newRoute !== routes.OS_RESET) {
      return;
    }

    this.attemptDeepLink();
  },
});
