// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {isRTL} from 'chrome://resources/ash/common/util.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './edu_coexistence_button.html.js';

enum ButtonTypes {
  ACTION = 'action',
  BACK = 'back',
}

const EduCoexistenceButtonBase = I18nMixin(PolymerElement);

export class EduCoexistenceButton extends EduCoexistenceButtonBase {
  static get is() {
    return 'edu-coexistence-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      buttonType: {
        type: String,
        value: ButtonTypes.ACTION,
      },

      buttonClasses: {
        type: String,
        computed: 'getClass(buttonType)',
      },

      disabled: {
        type: Boolean,
        value: false,
      },
    };
  }

  disabled: boolean;
  private buttonType: ButtonTypes;

  override ready() {
    super.ready();
    this.assertButtonType(this.buttonType);
  }

  private assertButtonType(buttonType: ButtonTypes) {
    assert(Object.values(ButtonTypes).includes(buttonType));
  }

  private getClass(buttonType: ButtonTypes): string {
    this.assertButtonType(buttonType);
    return buttonType === ButtonTypes.ACTION ? 'action-button' : '';
  }

  private hasIconBeforeText(buttonType: ButtonTypes): boolean {
    this.assertButtonType(buttonType);
    return buttonType === ButtonTypes.BACK;
  }

  private getIcon(buttonType: ButtonTypes): string {
    this.assertButtonType(buttonType);
    if (buttonType === ButtonTypes.BACK) {
      return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
    }
    return '';
  }

  private getDisplayName(buttonType: ButtonTypes): string {
    this.assertButtonType(buttonType);

    if (buttonType === ButtonTypes.BACK) {
      return this.i18n('backButton');
    }
    if (buttonType === ButtonTypes.ACTION) {
      return this.i18n('nextButton');
    }
    return '';  // unreached
  }

  private onClick(e: Event) {
    if (this.disabled) {
      e.stopPropagation();
      return;
    }
    if (this.buttonType === ButtonTypes.BACK) {
      this.dispatchEvent(new CustomEvent('go-back', {
        bubbles: true,
        composed: true,
      }));
      return;
    }
    if (this.buttonType === ButtonTypes.ACTION) {
      this.dispatchEvent(new CustomEvent('go-action', {
        bubbles: true,
        composed: true,
      }));
      return;
    }
  }
}

customElements.define(EduCoexistenceButton.is, EduCoexistenceButton);
