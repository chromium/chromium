// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_dialog_learn_more.html.js';

export interface PrivacySandboxDialogLearnMoreElement {
  $: {
    collapse: HTMLElement,
  };
}

export class PrivacySandboxDialogLearnMoreElement extends PolymerElement {
  static get is() {
    return 'privacy-sandbox-dialog-learn-more';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      title: String,
      expanded: {
        type: Boolean,
        observer: 'onExpandedChanged_',
        notify: true,
        value: false,
      },
    };
  }

  private onExpandedChanged_(expanded: boolean) {
    if (expanded) {
      this.scrollIntoCollapseElement_(this.$.collapse);
    }
  }

  private scrollIntoCollapseElement_(element: HTMLElement) {
    const computedStyle = window.getComputedStyle(element);
    const duration = parseFloat(
        computedStyle.getPropertyValue('--iron-collapse-transition-duration'));
    // Wait for collapse section transition to complete 70%.
    setTimeout(() => {
      // ...and scroll the content area up to make the section content
      // visible.
      element.scrollIntoView({block: 'start', behavior: 'smooth'});
    }, duration * 0.7);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-dialog-learn-more': PrivacySandboxDialogLearnMoreElement;
  }
}

customElements.define(
    PrivacySandboxDialogLearnMoreElement.is,
    PrivacySandboxDialogLearnMoreElement);
