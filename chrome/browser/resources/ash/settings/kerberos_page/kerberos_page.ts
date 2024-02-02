// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-page' is the settings page containing Kerberos Tickets
 * settings.
 */
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './kerberos_page.html.js';

const SettingsKerberosPageElementBase =
    RouteOriginMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class SettingsKerberosPageElement extends
    SettingsKerberosPageElementBase {
  static get is() {
    return 'settings-kerberos-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kKerberos,
        readOnly: true,
      },
    };
  }

  private section_: Section;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.KERBEROS;
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(
        routes.KERBEROS_ACCOUNTS_V2, '#kerberosAccountsSubpageTrigger');
  }

  private onKerberosAccountsClick_(): void {
    Router.getInstance().navigateTo(routes.KERBEROS_ACCOUNTS_V2);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsKerberosPageElement.is]: SettingsKerberosPageElement;
  }
}

customElements.define(
    SettingsKerberosPageElement.is, SettingsKerberosPageElement);
