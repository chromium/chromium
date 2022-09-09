// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-reset-page' is the OS settings page containing reset
 * settings.
 */
import './os_powerwash_dialog.js';

import {getEuicc, getNonPendingESimProfiles} from 'chrome://resources/cr_components/chromeos/cellular_setup/esim_manager_utils.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink_js.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const OsSettingsResetPageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, RouteObserverBehavior], PolymerElement);

/** @polymer */
class OsSettingsResetPageElement extends OsSettingsResetPageElementBase {
  static get is() {
    return 'os-settings-reset-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kPowerwash]),
      },
    };
  }

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
  }

  /** @private */
  onPowerwashDialogClose_() {
    this.showPowerwashDialog_ = false;
    focusWithoutInk(assert(this.$.powerwash));
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    // Does not apply to this page.
    if (newRoute !== routes.OS_RESET) {
      return;
    }

    this.attemptDeepLink();
  }
}

customElements.define(
    OsSettingsResetPageElement.is, OsSettingsResetPageElement);
