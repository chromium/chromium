// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {isRTL} from 'chrome://resources/ash/common/util.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
const ButtonTypes = {
  ACTION: 'action',
  BACK: 'back',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const EduCoexistenceButtonBase = mixinBehaviors([I18nBehavior], PolymerElement);

/**
 * @polymer
 */
class EduCoexistenceButton extends EduCoexistenceButtonBase {
  static get is() {
    return 'edu-coexistence-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set button type.
       */
      buttonType: {
        type: String,
        value: ButtonTypes.ACTION,
      },

      /**
       * Button class list string.
       */
      buttonClasses: {
        type: String,
        computed: 'getClass_(buttonType, newOobeStyleEnabled)',
      },

      /**
       * 'disabled' button attribute.
       */
      disabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether to use new OOBE style for the button.
       */
      newOobeStyleEnabled: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.assertButtonType_(this.buttonType);
  }

  /**
   * @param {string} buttonType
   * @private
   */
  assertButtonType_(buttonType) {
    assert(Object.values(ButtonTypes).includes(buttonType));
  }

  /**
   * @param {!ButtonTypes} buttonType
   * @param {boolean} newOobeStyleEnabled
   * @return {string} CSS class names
   * @private
   */
  getClass_(buttonType, newOobeStyleEnabled) {
    this.assertButtonType_(buttonType);

    // Disable the border if necessary.
    const cssClassses = newOobeStyleEnabled ? 'no-border button-radius' : '';

    if (buttonType === ButtonTypes.BACK) {
      return cssClassses;
    }

    return 'action-button ' + cssClassses;
  }

  /**
   * @param {!ButtonTypes} buttonType
   * @return {boolean} Whether the button should have an icon before text
   * @private
   */
  hasIconBeforeText_(buttonType) {
    this.assertButtonType_(buttonType);
    return buttonType === ButtonTypes.BACK;
  }

  /**
   * @param {!ButtonTypes} buttonType
   * @return {boolean} Whether the button should have an icon after text
   * @private
   */
  hasIconAfterText_(buttonType) {
    this.assertButtonType_(buttonType);
    return false;
  }

  /**
   * @param {!ButtonTypes} buttonType
   * @return {string} Icon
   * @private
   */
  getIcon_(buttonType) {
    this.assertButtonType_(buttonType);
    if (buttonType === ButtonTypes.BACK) {
      return isRTL() ? 'cr:chevron-right' : 'cr:chevron-left';
    }
    return '';
  }

  /**
   * @param {!ButtonTypes} buttonType
   * @return {string} Localized button text
   * @private
   */
  getDisplayName_(buttonType) {
    this.assertButtonType_(buttonType);

    if (buttonType === ButtonTypes.BACK) {
      return this.i18n('backButton');
    }
    if (buttonType === ButtonTypes.ACTION) {
      return this.i18n('nextButton');
    }
    return '';  // unreached
  }

  /**
   * @param {!Event} e
   * @private
   */
  onTap_(e) {
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
