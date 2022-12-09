// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-page' is the settings page containing Kerberos Tickets
 * settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';
import './kerberos_accounts.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Router} from '../router.js';
import {routes} from '../os_route.js';

import {getTemplate} from './kerberos_page.html.js';

const SettingsKerberosPageElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

class SettingsKerberosPageElement extends SettingsKerberosPageElementBase {
  static get is() {
    return 'settings-kerberos-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

  private focusConfig_: Map<string, string>;

  private onKerberosAccountsTap_(): void {
    Router.getInstance().navigateTo(routes.KERBEROS_ACCOUNTS_V2);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-kerberos-page': SettingsKerberosPageElement;
  }
}

customElements.define(
    SettingsKerberosPageElement.is, SettingsKerberosPageElement);
