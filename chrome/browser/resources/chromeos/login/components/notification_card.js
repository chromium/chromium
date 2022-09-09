// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* #js_imports_placeholder */

class NotificationCard extends Polymer.Element {

  static get is() {
    return 'notification-card';
  }

  /* #html_template_placeholder */

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
