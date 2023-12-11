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

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '../buttons/oobe_text_button.js';
import '../common_styles/oobe_common_styles.css.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeFocusBehavior, OobeFocusBehaviorInterface} from '../behaviors/oobe_focus_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../behaviors/oobe_i18n_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeFocusBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeModalDialogBase =
    mixinBehaviors([OobeFocusBehavior, OobeI18nBehavior], PolymerElement);

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
    return this.shadowRoot.querySelector('#modalDialog').open;
  }

  ready() {
    super.ready();
  }

  showDialog() {
    chrome.send('enableShelfButtons', [false]);
    this.shadowRoot.querySelector('#modalDialog').showModal();
    this.focusMarkedElement(this);
  }

  hideDialog() {
    this.shadowRoot.querySelector('#modalDialog').close();
  }

  onClose_() {
    chrome.send('enableShelfButtons', [true]);
  }
}

customElements.define(OobeModalDialog.is, OobeModalDialog);
