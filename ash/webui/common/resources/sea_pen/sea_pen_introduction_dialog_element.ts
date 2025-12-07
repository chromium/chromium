// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Displays an introduction dialog about the Sea Pen feature.
 */

import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import './sea_pen_introduction_svg_element.js';

import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isSeaPenTextInputEnabled} from './load_time_booleans.js';
import {getTemplate} from './sea_pen_introduction_dialog_element.html.js';

export class SeaPenIntroductionCloseEvent extends CustomEvent<null> {
  static readonly EVENT_NAME = 'sea-pen-introduction-dialog-close';

  constructor() {
    super(
        SeaPenIntroductionCloseEvent.EVENT_NAME,
        {
          bubbles: true,
          composed: true,
          detail: null,
        },
    );
  }
}

export class SeaPenFreeformIntroductionCloseEvent extends CustomEvent<null> {
  static readonly EVENT_NAME = 'sea-pen-freeform-introduction-dialog-close';

  constructor() {
    super(
        SeaPenFreeformIntroductionCloseEvent.EVENT_NAME,
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
    'sea-pen-introduction-dialog': SeaPenIntroductionDialogElement;
  }
}

export interface SeaPenIntroductionDialogElement {
  $: {dialog: CrDialogElement};
}

export class SeaPenIntroductionDialogElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'sea-pen-introduction-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onClickClose_() {
    this.$.dialog.cancel();
    if (isSeaPenTextInputEnabled()) {
      this.dispatchEvent(new SeaPenFreeformIntroductionCloseEvent());
    } else {
      this.dispatchEvent(new SeaPenIntroductionCloseEvent());
    }
  }

  private getIntroDialogContent_() {
    return isSeaPenTextInputEnabled() ?
        this.i18nAdvanced('seaPenFreeformIntroductionDialogContent') :
        this.i18nAdvanced('seaPenIntroductionDialogContent');
  }
}

customElements.define(
    SeaPenIntroductionDialogElement.is, SeaPenIntroductionDialogElement);
