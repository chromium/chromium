// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'manage-isolated-web-apps-page' is responsible for Isolated Web Apps related
 * controls.
 */

import '../../controls/settings_toggle_button.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './manage_isolated_web_apps_subpage.html.js';

const ManageIsolatedWebAppsSubpageBase =
    I18nMixin(WebUiListenerMixin(PolymerElement));

export class ManageIsolatedWebAppsSubpageElement extends
    ManageIsolatedWebAppsSubpageBase {
  static get is() {
    return 'settings-manage-isolated-web-apps-subpage';
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
    };
  }

  prefs: {[key: string]: any};
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-manage-isolated-web-apps-subpage':
        ManageIsolatedWebAppsSubpageElement;
  }
}

customElements.define(
    ManageIsolatedWebAppsSubpageElement.is,
    ManageIsolatedWebAppsSubpageElement);
