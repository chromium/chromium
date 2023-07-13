// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which shows and controls password sharing dialogs.
 */
import './share_password_loading_dialog.js';
import './share_password_error_dialog.js';
import './share_password_no_members_dialog.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './share_password_flow.html.js';


export enum ShareFlowState {
  NO_DIALOG,
  FETCHING,
  ERROR,
  NO_MEMBERS,
}

const SharePasswordFlowElementBase = I18nMixin(PolymerElement);

export class SharePasswordFlowElement extends SharePasswordFlowElementBase {
  static get is() {
    return 'share-password-flow';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      passwordName: String,

      flowState: Number,

      flowStateEnum_: {
        type: Object,
        value: ShareFlowState,
        readOnly: true,
      },
    };
  }

  passwordName: string;
  flowState: ShareFlowState = ShareFlowState.NO_DIALOG;

  override connectedCallback() {
    super.connectedCallback();

    this.startSharing_();
  }

  private startSharing_() {
    this.flowState = ShareFlowState.FETCHING;
    // TODO(1445526): Fetch recipients.
  }

  private isState_(state: ShareFlowState): boolean {
    return this.flowState === state;
  }

  private getShareDialogTitle_(): string {
    return this.i18n('shareDialogTitle', this.passwordName);
  }

  private onDialogClose_() {
    this.dispatchEvent(
        new CustomEvent('share-flow-done', {bubbles: true, composed: true}));
    this.flowState = ShareFlowState.NO_DIALOG;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-flow': SharePasswordFlowElement;
  }
}

customElements.define(SharePasswordFlowElement.is, SharePasswordFlowElement);
