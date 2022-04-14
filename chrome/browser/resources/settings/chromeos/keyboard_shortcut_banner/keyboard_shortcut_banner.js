// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'keyboard-shortcut-banner' is an element to display reminders
 * for keyboard shortcuts.
 *
 * The body of the reminder should be specified as an array of strings given by
 * the "body" property. Use a <kbd> element to wrap entire shortcuts, and use
 * nested <kbd> elements to signify keys. Do not add spaces around the + between
 * keyboard shortcuts. For example, "Press Ctrl + Space" should be passed in as
 * "Press <kbd><kbd>Ctrl</kbd>+<kbd>Space</kbd></kbd>".
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const KeyboardShortcutBannerBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class KeyboardShortcutBanner extends KeyboardShortcutBannerBase {
  static get is() {
    return 'keyboard-shortcut-banner';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      header: {
        type: String,
      },

      /** @type {!Array<string>} */
      body: {
        type: Array,
      }
    };
  }

  /** @private */
  onDismissClick_() {
    getAnnouncerInstance().announce(this.i18n('shortcutBannerDismissed'));
    this.dispatchEvent(new CustomEvent('dismiss'));
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getIdForIndex_(index) {
    return `id${index}`;
  }

  /**
   * @return {string}
   * @private
   */
  getBodyIds_() {
    const /** !Array<string> */ ids = [];
    for (let i = 0; i < this.body.length; i++) {
      ids.push(this.getIdForIndex_(i));
    }
    return ids.join(' ');
  }
}

customElements.define(KeyboardShortcutBanner.is, KeyboardShortcutBanner);
