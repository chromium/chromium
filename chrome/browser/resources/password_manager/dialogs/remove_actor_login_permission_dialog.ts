// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../shared_style.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './remove_actor_login_permission_dialog.html.js';

export interface RemoveActorLoginPermissionDialogElement {
  $: {
    dialog: CrDialogElement,
    disconnect: CrButtonElement,
    text: HTMLElement,
  };
}

const RemoveActorLoginPermissionDialogElementBase = I18nMixin(PolymerElement);

export class RemoveActorLoginPermissionDialogElement extends
    RemoveActorLoginPermissionDialogElementBase {
  static get is() {
    return 'remove-actor-login-permission-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The origin of the site.
       */
      origin: String,
    };
  }

  declare origin: string;

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onDisconnectClick_() {
    this.dispatchEvent(new CustomEvent(
        'remove-actor-login-permission-click',
        {bubbles: true, composed: true}));
    this.$.dialog.close();
  }

  private onCancelClick_() {
    this.$.dialog.close();
  }

  /**
   * Returns the description for the dialog.
   */
  private getDescriptionText_(): string {
    return this.i18n('removeActorLoginDialogDescription', this.origin);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'remove-actor-login-permission-dialog':
        RemoveActorLoginPermissionDialogElement;
  }
}
customElements.define(
    RemoveActorLoginPermissionDialogElement.is,
    RemoveActorLoginPermissionDialogElement);
