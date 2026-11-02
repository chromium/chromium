// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which shows and controls password sharing dialogs.
 */

import './share_password_family_picker_dialog.js';
import './share_password_loading_dialog.js';
import './share_password_error_dialog.js';
import './share_password_no_other_family_members_dialog.js';
import './share_password_not_family_member_dialog.js';
import './share_password_confirmation_dialog.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordManagerProxy} from '../password_manager_proxy.js';
import {PasswordManagerImpl} from '../password_manager_proxy.js';

import {getHtml} from './share_password_flow.html.js';

export enum ShareFlowState {
  NO_DIALOG,
  FETCHING,
  ERROR,
  NO_OTHER_MEMBERS,
  NOT_FAMILY_MEMBER,
  FAMILY_PICKER,
  CONFIRMATION,
}

const SharePasswordFlowElementBase = I18nMixinLit(CrLitElement);

export class SharePasswordFlowElement extends SharePasswordFlowElementBase {
  static get is() {
    return 'share-password-flow';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      passwordName: {type: String},
      iconUrl: {type: String},
      password: {type: Object},
      flowState: {type: Number},
      fetchResults_: {type: Object},
      recipients_: {type: Array},
    };
  }

  accessor passwordName: string = '';
  accessor iconUrl: string = '';
  accessor password: chrome.passwordsPrivate.PasswordUiEntry;
  accessor flowState: ShareFlowState = ShareFlowState.NO_DIALOG;
  protected accessor recipients_: chrome.passwordsPrivate.RecipientInfo[] = [];
  protected accessor fetchResults_: chrome.passwordsPrivate.FamilyFetchResults|
      null = null;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  override connectedCallback() {
    this.startSharing_();
    super.connectedCallback();
  }

  private async startSharing_() {
    // TODO(crbug.com/40268194): Add timeout to avoid flickering.
    this.flowState = ShareFlowState.FETCHING;

    this.fetchResults_ = await this.passwordManager_.fetchFamilyMembers();
    switch (this.fetchResults_.status) {
      case chrome.passwordsPrivate.FamilyFetchStatus.UNKNOWN_ERROR:
        this.flowState = ShareFlowState.ERROR;
        break;
      case chrome.passwordsPrivate.FamilyFetchStatus.NO_MEMBERS:
        // TODO(crbug.com/40268194): Rename FamilyFetchStatus.NO_MEMBERS to
        // NOT_FAMILY_MEMBER.
        this.flowState = ShareFlowState.NOT_FAMILY_MEMBER;
        break;
      case chrome.passwordsPrivate.FamilyFetchStatus.SUCCESS:
        if (this.fetchResults_.familyMembers.length === 0) {
          this.flowState = ShareFlowState.NO_OTHER_MEMBERS;
          return;
        }
        this.flowState = ShareFlowState.FAMILY_PICKER;
        break;
      default:
        assertNotReached();
    }
  }

  protected isState_(state: ShareFlowState): boolean {
    return this.flowState === state;
  }

  protected getShareDialogTitle_(): string {
    return this.i18n('shareDialogTitle', this.passwordName);
  }

  protected onDialogClose_() {
    this.fire('share-flow-done');
    this.flowState = ShareFlowState.NO_DIALOG;
  }

  protected onDialogCancel_() {
    this.onDialogClose_();
  }

  protected onRestart_() {
    this.startSharing_();
  }

  protected onStartShare_() {
    this.flowState = ShareFlowState.CONFIRMATION;
  }

  protected onSelectedRecipientsChanged_(
      e: CustomEvent<{value: chrome.passwordsPrivate.RecipientInfo[]}>) {
    this.recipients_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-flow': SharePasswordFlowElement;
  }
}

customElements.define(SharePasswordFlowElement.is, SharePasswordFlowElement);
