// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  Accordion,
} from 'chrome://resources/cros_components/accordion/accordion.js';
import {css} from 'chrome://resources/mwc/lit/index.js';

export class CraAccordion extends Accordion {
  static override styles = [
    Accordion.styles,
    css`
      ::slotted(cra-accordion-item:last-of-type) {
        --cros-accordion-item-separator-display: none;
      }
    `,
  ];
}

window.customElements.define('cra-accordion', CraAccordion);

declare global {
  interface HTMLElementTagNameMap {
    'cra-accordion': CraAccordion;
  }
}
