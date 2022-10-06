// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/smb_shares/add_smb_share_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {getTemplate} from './smb_shares_page.html.js';

const SettingsSmbSharesPageElementBase =
    mixinBehaviors([RouteObserverBehavior], PolymerElement) as {
      new (): PolymerElement & RouteObserverBehaviorInterface,
    };

class SettingsSmbSharesPageElement extends SettingsSmbSharesPageElementBase {
  static get is() {
    return 'settings-smb-shares-page';
  }

  static get template() {
    return getTemplate();
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

      showAddSmbDialog_: Boolean,
    };
  }

  prefs: object;
  private showAddSmbDialog_: boolean;

  /**
   * Overridden from RouteObserverBehavior.
   */
  override currentRouteChanged(route: Route) {
    if (route === routes.SMB_SHARES) {
      this.showAddSmbDialog_ = Router.getInstance().getQueryParameters().get(
                                   'showAddShare') === 'true';
    }
  }

  private onAddShareTap_() {
    this.showAddSmbDialog_ = true;
  }

  private onAddSmbDialogClosed_() {
    this.showAddSmbDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-smb-shares-page': SettingsSmbSharesPageElement;
  }
}

customElements.define(
    SettingsSmbSharesPageElement.is, SettingsSmbSharesPageElement);
