// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-welcome-fragment' is the fragment in a privacy guide
 * card that contains the welcome screen and its description.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './privacy_guide_fragment_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_guide_welcome_fragment.html.js';

export interface PrivacyGuideWelcomeFragmentElement {
  $: {
    startButton: HTMLElement,
  };
}

export class PrivacyGuideWelcomeFragmentElement extends PolymerElement {
  static get is() {
    return 'privacy-guide-welcome-fragment';
  }

  static get template() {
    return getTemplate();
  }

  override focus() {
    this.shadowRoot!.querySelector<HTMLElement>('.headline')!.focus();
  }

  private onStartButtonClick_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(
        new CustomEvent('start-button-click', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-welcome-fragment': PrivacyGuideWelcomeFragmentElement;
  }
}

customElements.define(
    PrivacyGuideWelcomeFragmentElement.is, PrivacyGuideWelcomeFragmentElement);
