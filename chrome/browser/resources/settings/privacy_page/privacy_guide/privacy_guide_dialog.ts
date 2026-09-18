// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-rivacy-guide-dialog' is a settings dialog that helps users guide
 * various privacy settings.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './privacy_guide_page.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './privacy_guide_dialog.css.js';
import {getHtml} from './privacy_guide_dialog.html.js';

export interface SettingsPrivacyGuideDialogElement {
  $: {
    backToSettingsButton: HTMLElement,
    dialog: HTMLDialogElement,
  };
}

export class SettingsPrivacyGuideDialogElement extends CrLitElement {
  static get is() {
    return 'settings-privacy-guide-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override firstUpdated() {
    this.$.dialog.showModal();
    this.$.backToSettingsButton.focus();
  }

  protected onDialogCancel_(e: Event) {
    if (e.target === this.$.dialog) {
      e.preventDefault();
    }
  }

  protected onDialogClose_(e: Event) {
    // Ignore any 'close' events not fired directly by the <dialog> element.
    if (e.target !== this.$.dialog) {
      return;
    }

    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.fire('close');
  }

  protected onPrivacyGuidePageClose_(e: Event) {
    e.stopPropagation();
    this.$.dialog.close();
  }

  protected onSettingsBackClick_(e: Event) {
    e.stopPropagation();

    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-guide-dialog': SettingsPrivacyGuideDialogElement;
  }
}

customElements.define(
    SettingsPrivacyGuideDialogElement.is, SettingsPrivacyGuideDialogElement);
