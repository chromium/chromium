// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/icons.html.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import './oobe_icons.m.js';
import './common_styles/oobe_common_styles.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class NotificationCard extends PolymerElement {
  static get is() {
    return 'notification-card';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      buttonLabel: {type: String, value: ''},

      linkLabel: {type: String, value: ''},
    };
  }

  /** @private */
  buttonClicked_() {
    this.dispatchEvent(new CustomEvent('buttonclick',
        { bubbles: true, composed: true }));
  }

  /**
   * @param {Event} e
   * @private
   */
  linkClicked_(e) {
    this.dispatchEvent(new CustomEvent('linkclick',
        { bubbles: true, composed: true }));
    e.preventDefault();
  }

  /** @type {Element} */
  get submitButton() {
    return this.$.submitButton;
  }
}

customElements.define(NotificationCard.is, NotificationCard);
