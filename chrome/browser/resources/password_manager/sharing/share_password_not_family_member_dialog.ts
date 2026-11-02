// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './metrics_utils.js';
import './share_password_dialog_header.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordSharingActions, recordPasswordSharingInteraction} from './metrics_utils.js';
import {getCss} from './share_password_not_family_member_dialog.css.js';
import {getHtml} from './share_password_not_family_member_dialog.html.js';

export interface SharePasswordNotFamilyMemberDialogElement {
  $: {
    description: HTMLElement,
    action: HTMLElement,
    header: HTMLElement,
  };
}

const SharePasswordNotFamilyMemberDialogElementBase =
    I18nMixinLit(CrLitElement);

export class SharePasswordNotFamilyMemberDialogElement extends
    SharePasswordNotFamilyMemberDialogElementBase {
  static get is() {
    return 'share-password-not-family-member-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dialogTitle: {type: String},
    };
  }

  accessor dialogTitle: string = '';

  protected onDescriptionClick_(e: Event) {
    const element = e.target as HTMLElement;
    if (element.tagName === 'A') {
      recordPasswordSharingInteraction(
          PasswordSharingActions.NOT_FAMILY_MEMBER_CREATE_FAMILY_CLICKED);
    }
  }

  protected onActionButtonClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.NOT_FAMILY_MEMBER_GOT_IT_CLICKED);
    this.fire('close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-not-family-member-dialog':
        SharePasswordNotFamilyMemberDialogElement;
  }
}

customElements.define(
    SharePasswordNotFamilyMemberDialogElement.is,
    SharePasswordNotFamilyMemberDialogElement);
