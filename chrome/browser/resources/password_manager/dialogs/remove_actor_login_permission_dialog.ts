// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {getCss as getCrSharedStyleCss} from 'chrome://resources/cr_elements/cr_shared_style_lit.css.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss as getSharedStyleCss} from '../shared_style_lit.css.js';

import {getHtml} from './remove_actor_login_permission_dialog.html.js';

export interface RemoveActorLoginPermissionDialogElement {
  $: {
    dialog: CrDialogElement,
    disconnect: CrButtonElement,
    text: HTMLElement,
  };
}

const RemoveActorLoginPermissionDialogElementBase = I18nMixinLit(CrLitElement);

export class RemoveActorLoginPermissionDialogElement extends
    RemoveActorLoginPermissionDialogElementBase {
  static get is() {
    return 'remove-actor-login-permission-dialog';
  }

  static override get styles() {
    return [
      getSharedStyleCss(),
      getCrSharedStyleCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The origin of the site.
       */
      origin: {type: String},
    };
  }

  accessor origin: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  protected onDisconnectClick_() {
    this.fire('remove-actor-login-permission-click');
    this.$.dialog.close();
  }

  protected onCancelClick_() {
    this.$.dialog.close();
  }

  /**
   * Returns the description for the dialog.
   */
  protected getDescriptionText_(): string {
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
