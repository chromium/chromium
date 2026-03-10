// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_page/settings_subpage.js';
import '../simple_confirmation_dialog.js';
import '../site_favicon.js';

import {assert} from '//resources/js/assert.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsSimpleConfirmationDialogElement} from '../simple_confirmation_dialog.js';

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

      selectedPermissionToRemove_: {
        type: Object,
        value: null,
      },
    };
  }

  declare private actorLoginPermissions_: LoginPermission[];
  declare private selectedPermissionToRemove_: LoginPermission|null;

  private onRemoveActorLoginPermissionClick_(
      e: DomRepeatEvent<LoginPermission>) {
    this.selectedPermissionToRemove_ = e.model.item;
  }

  private onRemoveDialogClose_() {
    const dialog =
        this.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            'settings-simple-confirmation-dialog');
    assert(dialog);
    if (dialog.wasConfirmed()) {
      // TODO(crbug.com/481214101): Implement remove logic.
    }
    this.selectedPermissionToRemove_ = null;
  }

  private getRemoveDialogDescription_(url: string): string {
    return this.i18n('glicRemoveActorLoginDialogDescription', url);
  }
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
