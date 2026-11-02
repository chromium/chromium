// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './metrics_utils.js';
import './share_password_dialog_header.js';
import './share_password_recipient.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {UserUtilMixinLit} from '../user_utils_mixin_lit.js';

import {PasswordSharingActions, recordPasswordSharingInteraction} from './metrics_utils.js';
import {getCss} from './share_password_family_picker_dialog.css.js';
import {getHtml} from './share_password_family_picker_dialog.html.js';

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

const SharePasswordFamilyPickerDialogElementBase =
    UserUtilMixinLit(I18nMixinLit(CrLitElement));

export class SharePasswordFamilyPickerDialogElement extends
    SharePasswordFamilyPickerDialogElementBase {
  static get is() {
    return 'share-password-family-picker-dialog';
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
      members: {type: Array},

      selectedRecipients: {
        type: Array,
        notify: true,
      },

      eligibleRecipients_: {type: Array},
      ineligibleRecipients_: {type: Array},
    };
  }

  accessor dialogTitle: string = '';
  accessor members: chrome.passwordsPrivate.RecipientInfo[] = [];
  accessor selectedRecipients: chrome.passwordsPrivate.RecipientInfo[] = [];
  protected accessor eligibleRecipients_:
      chrome.passwordsPrivate.RecipientInfo[] = [];
  protected accessor ineligibleRecipients_:
      chrome.passwordsPrivate.RecipientInfo[] = [];

  override connectedCallback() {
    super.connectedCallback();

    recordPasswordSharingInteraction(
        PasswordSharingActions.FAMILY_PICKER_OPENED);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('members')) {
      this.eligibleRecipients_ = this.computeEligible_();
      this.ineligibleRecipients_ = this.computeIneligible_();

      // Pre-select the member if they are eligible for sharing and there are no
      // other members in the group.
      if (this.members.length === 1 && this.eligibleRecipients_.length === 1) {
        this.selectedRecipients = this.members;
      }
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

  protected onRecipientChange_(): void {
    this.selectedRecipients =
        Array.from(this.shadowRoot.querySelectorAll('share-password-recipient'))
            .filter(item => item.selected)
            .map(item => item.recipient);
  }

  // Should only be called for eligible recipients.
  protected shouldPreselectFirstEntry_(index: number): boolean {
    // Only pre-select the first entry when there is only single group member.
    return index === 0 && this.members.length === 1;
  }

  protected onViewFamilyClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.FAMILY_PICKER_VIEW_FAMILY_CLICKED);
  }

  protected onCancelClick_() {
    recordPasswordSharingInteraction(
        PasswordSharingActions.FAMILY_PICKER_CANCELED);
    this.fire('close');
  }

  protected onShareClick_() {
    if (this.selectedRecipients.length === 1) {
      recordPasswordSharingInteraction(
          PasswordSharingActions.FAMILY_PICKER_SHARE_WITH_ONE_MEMBER);
    } else {
      recordPasswordSharingInteraction(
          PasswordSharingActions.FAMILY_PICKER_SHARE_WITH_MULTIPLE_MEMBERS);
    }
    this.fire('start-share');
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
