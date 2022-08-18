// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/chromeos/smb_shares/add_smb_share_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsSmbSharesPageElementBase =
    mixinBehaviors([RouteObserverBehavior], PolymerElement);

/** @polymer */
class SettingsSmbSharesPageElement extends SettingsSmbSharesPageElementBase {
  static get is() {
    return 'settings-smb-shares-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /** @private */
      showAddSmbDialog_: Boolean,
    };
  }

  /**
   * Overridden from RouteObserverBehavior.
   * @param {!Route} route
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (route === routes.SMB_SHARES) {
      this.showAddSmbDialog_ = Router.getInstance().getQueryParameters().get(
                                   'showAddShare') === 'true';
    }
  }

  /** @private */
  onAddShareTap_() {
    this.showAddSmbDialog_ = true;
  }

  /** @private */
  onAddSmbDialogClosed_() {
    this.showAddSmbDialog_ = false;
  }
}

customElements.define(
    SettingsSmbSharesPageElement.is, SettingsSmbSharesPageElement);
