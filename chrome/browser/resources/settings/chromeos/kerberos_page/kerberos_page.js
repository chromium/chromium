// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-page' is the settings page containing Kerberos Tickets
 * settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import './kerberos_accounts.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Router} from '../../router.js';
import {routes} from '../os_route.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsKerberosPageElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
class SettingsKerberosPageElement extends SettingsKerberosPageElementBase {
  static get is() {
    return 'settings-kerberos-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Map<string, string>} */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.KERBEROS_ACCOUNTS_V2) {
            map.set(
                routes.KERBEROS_ACCOUNTS_V2.path,
                '#kerberos-accounts-subpage-trigger');
          }
          return map;
        },
      },
    };
  }

  /** @private */
  onKerberosAccountsTap_() {
    Router.getInstance().navigateTo(routes.KERBEROS_ACCOUNTS_V2);
  }
}

customElements.define(
    SettingsKerberosPageElement.is, SettingsKerberosPageElement);
