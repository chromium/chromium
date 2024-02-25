// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/smb_shares/add_smb_share_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/ash/common/cr_elements/action_link.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './smb_shares_page.html.js';

const SettingsSmbSharesPageElementBase = RouteObserverMixin(PolymerElement);

export class SettingsSmbSharesPageElement extends
    SettingsSmbSharesPageElementBase {
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
   * Overridden from RouteObserverMixin.
   */
  override currentRouteChanged(route: Route): void {
    if (route === routes.SMB_SHARES) {
      this.showAddSmbDialog_ = Router.getInstance().getQueryParameters().get(
                                   'showAddShare') === 'true';
    }
  }

  private onAddShareClick_(): void {
    this.showAddSmbDialog_ = true;
  }

  private onAddSmbDialogClosed_(): void {
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
