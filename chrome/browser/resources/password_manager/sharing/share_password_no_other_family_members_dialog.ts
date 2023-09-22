// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './metrics_utils.js';
import './share_password_dialog_header.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordSharingActions, recordPasswordSharingInteraction} from './metrics_utils.js';
import {getTemplate} from './share_password_no_other_family_members_dialog.html.js';

export interface SharePasswordNoOtherFamilyMembersDialogElement {
  $: {
    description: HTMLElement,
    action: HTMLElement,
    header: HTMLElement,
  };
}

export class SharePasswordNoOtherFamilyMembersDialogElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'share-password-no-other-family-members-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dialogTitle: String,
    };
  }

  dialogTitle: string;

  private onDescriptionClick_(e: Event) {
    const element = e.target as HTMLElement;
    if (element.tagName === 'A') {
      recordPasswordSharingInteraction(
          PasswordSharingActions.NO_OTHER_FAMILY_MEMBERS_INVITE_LINK_CLICKED);
    }
  }

  private onClickActionButton_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.NO_OTHER_FAMILY_MEMBERS_GOT_IT_CLICKED);
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
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
