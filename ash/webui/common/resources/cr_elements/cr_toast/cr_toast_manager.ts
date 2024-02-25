// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which shows toasts with optional undo button.
 *
 * Forked from ui/webui/resources/cr_elements/cr_toast/cr_toast_manager.ts
 */

import './cr_toast.js';
import '../cr_hidden_style.css.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrToastElement} from './cr_toast.js';
import {getTemplate} from './cr_toast_manager.html.js';

let toastManagerInstance: CrToastManagerElement|null = null;

export function getToastManager(): CrToastManagerElement {
  assert(toastManagerInstance);
  return toastManagerInstance;
}

function setInstance(instance: CrToastManagerElement|null) {
  assert(!instance || !toastManagerInstance);
  toastManagerInstance = instance;
}

export interface CrToastManagerElement {
  $: {
    content: HTMLElement,
    slotted: HTMLSlotElement,
    toast: CrToastElement,
  };
}

export class CrToastManagerElement extends PolymerElement {
  static get is() {
    return 'cr-toast-manager';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      duration: {
        type: Number,
        value: 0,
      },
    };
  }

  duration: number;

  get isToastOpen(): boolean {
    return this.$.toast.open;
  }

  get slottedHidden(): boolean {
    return this.$.slotted.hidden;
  }

  override connectedCallback() {
    super.connectedCallback();

    setInstance(this);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    setInstance(null);
  }

  /**
   * @param label The label to display inside the toast.
   */
  show(label: string, hideSlotted: boolean = false) {
    this.$.content.textContent = label;
    this.showInternal_(hideSlotted);
  }

  /**
   * Shows the toast, making certain text fragments collapsible.
   */
  showForStringPieces(
      pieces: Array<{value: string, collapsible: boolean}>,
      hideSlotted: boolean = false) {
    const content = this.$.content;
    content.textContent = '';
    pieces.forEach(function(p) {
      if (p.value.length === 0) {
        return;
      }

      const span = document.createElement('span');
      span.textContent = p.value;
      if (p.collapsible) {
        span.classList.add('collapsible');
      }

      content.appendChild(span);
    });

    this.showInternal_(hideSlotted);
  }

  private showInternal_(hideSlotted: boolean) {
    this.$.slotted.hidden = hideSlotted;
    this.$.toast.show();
  }

  hide() {
    this.$.toast.hide();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-toast-manager': CrToastManagerElement;
  }
}

customElements.define(CrToastManagerElement.is, CrToastManagerElement);
