// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './metrics_utils.js';
import './share_password_dialog_header.js';
import './share_password_recipient.js';
import '../shared_style.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {UserUtilMixin} from '../user_utils_mixin.js';

import {PasswordSharingActions, recordPasswordSharingInteraction} from './metrics_utils.js';
import {getTemplate} from './share_password_family_picker_dialog.html.js';

export interface SharePasswordFamilyPickerDialogElement {
  $: {
    header: HTMLElement,
    description: HTMLElement,
    action: HTMLButtonElement,
    cancel: HTMLElement,
    avatar: HTMLImageElement,
    viewFamily: HTMLAnchorElement,
    footerDescription: HTMLElement,
  };
}

export class SharePasswordFamilyPickerDialogElement extends UserUtilMixin
(I18nMixin(PolymerElement)) {
  static get is() {
    return 'share-password-family-picker-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dialogTitle: String,

      members: {
        type: Array,
        value: [],
      },

      selectedRecipients: {
        type: Array,
        value: [],
        reflectToAttribute: true,
        notify: true,
      },

      eligibleRecipients_: {
        type: Array,
        computed: 'computeEligible_(members)',
      },

      ineligibleRecipients_: {
        type: Array,
        computed: 'computeIneligible_(members)',
      },
    };
  }

  dialogTitle: string;
  members: chrome.passwordsPrivate.RecipientInfo[];
  selectedRecipients: chrome.passwordsPrivate.RecipientInfo[];
  private eligibleRecipients_: chrome.passwordsPrivate.RecipientInfo[];
  private ineligibleRecipients_: chrome.passwordsPrivate.RecipientInfo[];

    override ready() {
      super.ready();

      recordPasswordSharingInteraction(
          PasswordSharingActions.FAMILY_PICKER_OPENED);

      // Pre-select the member if they are eligible for sharing and there are no
      // other members in the group.
      if (this.members.length === 1 && this.computeEligible_().length === 1) {
        this.selectedRecipients = this.members;
      }
    }

  private computeEligible_(): chrome.passwordsPrivate.RecipientInfo[] {
    const eligibleMembers = this.members.filter(member => member.isEligible);
    eligibleMembers.sort((a, b) => (a.displayName > b.displayName ? 1 : -1));
    return eligibleMembers;
  }

  private computeIneligible_(): chrome.passwordsPrivate.RecipientInfo[] {
    const inEligibleMembers = this.members.filter(member => !member.isEligible);
    inEligibleMembers.sort((a, b) => (a.displayName > b.displayName ? 1 : -1));
    return inEligibleMembers;
  }

  private recipientSelected_(): void {
    this.selectedRecipients =
        Array
            .from(this.shadowRoot!.querySelectorAll('share-password-recipient'))
            .filter(item => item.selected)
            .map(item => item.recipient);
  }

  // Should only be called for eligible recipients.
  private shouldPreselectFirstEntry_(index: number): boolean {
    // Only pre-select the first entry when there is only single group member.
    return index === 0 && this.members.length === 1;
  }

  private onViewFamilyClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.FAMILY_PICKER_VIEW_FAMILY_CLICKED);
  }

  private onClickCancel_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.FAMILY_PICKER_CANCELED);
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }

  private onClickShare_() {
    if (this.selectedRecipients.length === 1) {
      recordPasswordSharingInteraction(
          PasswordSharingActions.FAMILY_PICKER_SHARE_WITH_ONE_MEMBER);
    } else {
      recordPasswordSharingInteraction(
          PasswordSharingActions.FAMILY_PICKER_SHARE_WITH_MULTIPLE_MEMBERS);
    }
    this.dispatchEvent(
        new CustomEvent('start-share', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-family-picker-dialog':
        SharePasswordFamilyPickerDialogElement;
  }
}

customElements.define(
    SharePasswordFamilyPickerDialogElement.is,
    SharePasswordFamilyPickerDialogElement);
