// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * OOBE Modal Dialog
 *
 * Implements the 'OOBE Modal Dialog' according to MD specs.
 *
 * The dialog provides two properties that can be set directly from HTML.
 *  - titleKey - ID of the localized string to be used for the title.
 *  - contentKey - ID of the localized string to be used for the content.
 *
 *  Alternatively, one can set their own title and content into the 'title'
 *  and 'content' slots.
 *
 *  Buttons are optional and go into the 'buttons' slot. If none are specified,
 *  a default button with the text 'Close' will be shown. Users might want to
 *  trigger some action on their side by using 'on-close=myMethod'.
 */

import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '../buttons/oobe_text_button.js';
import '../common_styles/oobe_common_styles.m.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../behaviors/oobe_i18n_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeModalDialogBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/** @polymer */
export class OobeModalDialog extends OobeModalDialogBase {
  static get template() {
    return html`{__html_template__}`;
  }

  static get is() {
    return 'oobe-modal-dialog';
  }

  static get properties() {
    return {
      /* The ID of the localized string to be used as title text when no "title"
       * slot elements are specified.
       */
      titleKey: {
        type: String,
      },
      /* The ID of the localized string to be used as the content when no
       * "content" slot elements are specified.
       */
      contentKey: {
        type: String,
      },

      /**
       * True if close button should be hidden.
       * @type {boolean}
       */
      shouldHideCloseButton: {
        type: Boolean,
        value: false,
      },

      /**
       * True if title row should be hidden.
       * @type {boolean}
       */
      shouldHideTitleRow: {
        type: Boolean,
        value: false,
      },

      /**
       * True if confirmation dialog backdrop should be hidden.
       * @type {boolean}
       */
      shouldHideBackdrop: {
        type: Boolean,
        value: false,
      },
    };
  }

  get open() {
    return this.$.modalDialog.open;
  }

  ready() {
    super.ready();
  }

  /* Shows the modal dialog and changes the focus to the close button. */
  showDialog() {
    chrome.send('enableShelfButtons', [false]);
    this.$.modalDialog.showModal();
    this.$.closeButton.focus();
  }

  hideDialog() {
    this.$.modalDialog.close();
  }

  onClose_() {
    chrome.send('enableShelfButtons', [true]);
  }
}

customElements.define(OobeModalDialog.is, OobeModalDialog);
