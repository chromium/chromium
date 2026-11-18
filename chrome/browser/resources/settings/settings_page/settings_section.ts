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

import {CrLitElement, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './settings_section.css.js';
import {getHtml} from './settings_section.html.js';

export class SettingsSectionElement extends CrLitElement {
  static get is() {
    return 'settings-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Title for the section header. Initialize so we can use the
       * getTitleHiddenStatus_ method for accessibility.
       */
      pageTitle: {type: String},

      /**
       * When this attribute is enabled, a send feedback button will be shown
       * that emits a 'send-feedback' event.
       */
      showSendFeedbackButton: {type: Boolean},

      settingsRefresh2026Enabled_: {type: Boolean},
    };
  }

  accessor pageTitle: string = '';
  accessor showSendFeedbackButton: boolean = false;
  protected accessor settingsRefresh2026Enabled_: boolean =
      loadTimeData.getString('settingsRefresh2026') !== '';

  /**
   * Get the value to which to set the aria-hidden attribute of the section
   * heading.
   */
  protected getTitleHiddenStatus_(): string|typeof nothing {
    return this.pageTitle ? nothing : 'true';
  }

  override focus() {
    this.shadowRoot.querySelector<HTMLElement>('.title')!.focus();
  }

  protected onSendFeedbackClick_() {
    this.fire('send-feedback');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-section': SettingsSectionElement;
  }
}

customElements.define(SettingsSectionElement.is, SettingsSectionElement);
