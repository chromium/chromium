// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-rivacy-guide-dialog' is a settings dialog that helps users guide
 * various privacy settings.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import '../../settings_shared.css.js';
import './privacy_guide_page.js';

import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_guide_dialog.html.js';

export interface SettingsPrivacyGuideDialogElement {
  $: {
    dialog: HTMLDialogElement,
  };
}

export class SettingsPrivacyGuideDialogElement extends PolymerElement {
  static get is() {
    return 'settings-privacy-guide-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();

    const elementToFocus =
        this.shadowRoot!.querySelector<HTMLElement>('#backToSettingsButton')!;
    afterNextRender(this, () => elementToFocus!.focus());
  }

  private onDialogCancel_(e: Event) {
    if (e.target === this.$.dialog) {
      e.preventDefault();
    }
  }

  private onDialogClose_(e: Event) {
    // Ignore any 'close' events not fired directly by the <dialog> element.
    if (e.target !== this.$.dialog) {
      return;
    }

    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }

  private onPrivacyGuidePageClose_(e: Event) {
    e.stopPropagation();
    this.$.dialog.close();
  }

  private onSettingsBackClick_(e: Event) {
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
