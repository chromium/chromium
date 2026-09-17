// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-welcome-fragment' is the fragment in a privacy guide
 * card that contains the welcome screen and its description.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './privacy_guide_welcome_fragment.css.js';
import {getHtml} from './privacy_guide_welcome_fragment.html.js';

export interface PrivacyGuideWelcomeFragmentElement {
  $: {
    startButton: HTMLElement,
  };
}

export class PrivacyGuideWelcomeFragmentElement extends CrLitElement {
  static get is() {
    return 'privacy-guide-welcome-fragment';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override focus() {
    const header = this.shadowRoot.querySelector<HTMLElement>(
        '.welcome-completion-header-label');
    assert(header);
    header.focus();
  }

  protected onStartButtonClick_(e: Event) {
    e.stopPropagation();
    this.fire('start-button-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-welcome-fragment': PrivacyGuideWelcomeFragmentElement;
  }
}

customElements.define(
    PrivacyGuideWelcomeFragmentElement.is, PrivacyGuideWelcomeFragmentElement);
