// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_page/settings_subpage.js';
import '../site_favicon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './glic_login_permissions_page.html.js';

// TODO(crbug.com/481214101): Update with proper mojo types.
export interface LoginPermission {
  url: string;
  username: string;
}

const SettingsGlicLoginPermissionsPageElementBase = I18nMixin(PolymerElement);

export class SettingsGlicLoginPermissionsPageElement extends
    SettingsGlicLoginPermissionsPageElementBase {
  static get is() {
    return 'settings-glic-login-permissions-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      actorLoginPermissions_: {
        type: Array,
        value: () => [],
      },
    };
  }

  declare private actorLoginPermissions_: LoginPermission[];
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-glic-login-permissions-page':
        SettingsGlicLoginPermissionsPageElement;
  }
}

customElements.define(
    SettingsGlicLoginPermissionsPageElement.is,
    SettingsGlicLoginPermissionsPageElement);
