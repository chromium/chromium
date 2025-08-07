// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './text_copy_button.html.js';

export class TextCopyButton extends CustomElement {
  private revertIconTimeoutId_: number|null = null;
  readonly revertIconWaitDuration = 3000;
  private textToCopy: string = '';

  static override get template() {
    return getTemplate();
  }

  static get observedAttributes() {
    return ['text-to-copy'];
  }

  constructor() {
    super();
    this.onClickHandler = this.onClickHandler.bind(this);
  }

  connectedCallback() {
    this.addEventListener('click', this.onClickHandler);
    this.textToCopy = this.getAttribute('text-to-copy') || '';
  }

  disconnectedCallback() {
    this.removeEventListener('click', this.onClickHandler);
  }

  attributeChangedCallback(
      name: string, oldValue: string|null, newValue: string|null) {
    if (name === 'text-to-copy' && newValue !== oldValue) {
      this.textToCopy = newValue || '';
    }
  }

  async onClickHandler() {
    {
      try {
        await navigator.clipboard.writeText(this.textToCopy);
        this.setAttribute('text-recently-copied', '');

        if (this.revertIconTimeoutId_) {
          clearTimeout(this.revertIconTimeoutId_);
        }

        this.revertIconTimeoutId_ = setTimeout(() => {
          this.removeAttribute('text-recently-copied');
          this.revertIconTimeoutId_ = null;
        }, this.revertIconWaitDuration);
      } catch (err) {
        console.error('Failed to copy: ', err);
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'text-copy-button': TextCopyButton;
  }
}

customElements.define('text-copy-button', TextCopyButton);
