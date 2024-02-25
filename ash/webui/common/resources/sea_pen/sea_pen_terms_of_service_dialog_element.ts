// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Displays a dialog asking the user to review the Sea Pen
 * wallpaper terms before accessing Sea Pen feature.
 */

import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sea_pen_terms_of_service_dialog_element.html.js';

export class SeaPenTermsAcceptEvent extends CustomEvent<null> {
  static readonly EVENT_NAME = 'sea-pen-terms-dialog-accept';

  constructor() {
    super(
        SeaPenTermsAcceptEvent.EVENT_NAME,
        {
          bubbles: true,
          composed: true,
          detail: null,
        },
    );
  }
}

export class SeaPenTermsRefuseEvent extends CustomEvent<null> {
  static readonly EVENT_NAME = 'sea-pen-terms-dialog-refuse';

  constructor() {
    super(
        SeaPenTermsRefuseEvent.EVENT_NAME,
        {
          bubbles: true,
          composed: true,
          detail: null,
        },
    );
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sea-pen-wallpaper-terms-dialog': SeaPenTermsOfServiceDialogElement;
  }
}

export interface SeaPenTermsOfServiceDialogElement {
  $: {dialog: CrDialogElement};
}

export class SeaPenTermsOfServiceDialogElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'sea-pen-terms-of-service-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onClickAccept_() {
    this.$.dialog.cancel();
    this.dispatchEvent(new SeaPenTermsAcceptEvent());
  }

  private onClickRefuse_() {
    this.dispatchEvent(new SeaPenTermsRefuseEvent());
  }
}

customElements.define(
    SeaPenTermsOfServiceDialogElement.is, SeaPenTermsOfServiceDialogElement);
