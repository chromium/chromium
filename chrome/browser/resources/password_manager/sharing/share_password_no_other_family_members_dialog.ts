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
import {getCss} from './share_password_no_other_family_members_dialog.css.js';
import {getHtml} from './share_password_no_other_family_members_dialog.html.js';

export interface SharePasswordNoOtherFamilyMembersDialogElement {
  $: {
    description: HTMLElement,
    action: HTMLElement,
    header: HTMLElement,
  };
}

const SharePasswordNoOtherFamilyMembersDialogElementBase =
    I18nMixinLit(CrLitElement);

export class SharePasswordNoOtherFamilyMembersDialogElement extends
    SharePasswordNoOtherFamilyMembersDialogElementBase {
  static get is() {
    return 'share-password-no-other-family-members-dialog';
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
          PasswordSharingActions.NO_OTHER_FAMILY_MEMBERS_INVITE_LINK_CLICKED);
    }
  }

  protected onActionButtonClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.NO_OTHER_FAMILY_MEMBERS_GOT_IT_CLICKED);
    this.fire('close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-no-other-family-members-dialog':
        SharePasswordNoOtherFamilyMembersDialogElement;
  }
}

customElements.define(
    SharePasswordNoOtherFamilyMembersDialogElement.is,
    SharePasswordNoOtherFamilyMembersDialogElement);
