// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-section' shows a paper material themed section with a header
 * which shows its page title.
 *
 * Example:
 *
 *    <settings-section page-title="[[pageTitle]]">
 *      <!-- Insert your section controls here -->
 *    </settings-section>
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_section.html.js';

export class SettingsSectionElement extends PolymerElement {
  static get is() {
    return 'settings-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Title for the section header. Initialize so we can use the
       * getTitleHiddenStatus_ method for accessibility.
       */
      pageTitle: {
        type: String,
        value: '',
      },

      /**
       * When this attribute is enabled, a send feedback button will be shown
       * that emits a 'send-feedback' event.
       */
      showSendFeedbackButton: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare pageTitle: string;
  declare showSendFeedbackButton: boolean;

  /**
   * Get the value to which to set the aria-hidden attribute of the section
   * heading.
   * @return A return value of false will not add aria-hidden while aria-hidden
   *    requires a string of 'true' to be hidden as per aria specs. This
   *    function ensures we have the right return type.
   */
  private getTitleHiddenStatus_(): boolean|string {
    return this.pageTitle ? false : 'true';
  }

  override focus() {
    this.shadowRoot!.querySelector<HTMLElement>('.title')!.focus();
  }

  private onSendFeedbackClick_() {
    this.dispatchEvent(
        new CustomEvent('send-feedback', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);
