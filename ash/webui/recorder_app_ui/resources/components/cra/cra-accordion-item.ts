// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  AccordionItem,
} from 'chrome://resources/cros_components/accordion/accordion_item.js';
import {css} from 'chrome://resources/mwc/lit/index.js';

export class CraAccordionItem extends AccordionItem {
  static override styles = [
    AccordionItem.styles,
    // TODO: b/338544996 - Export the arrow via ::part, so we don't need this
    // workaround.
    css`
      :host(.hide-content) .accordion-row > cros-icon-button {
        display: none;
      }
    `,
  ];
}

window.customElements.define('cra-accordion-item', CraAccordionItem);

declare global {
  interface HTMLElementTagNameMap {
    'cra-accordion-item': CraAccordionItem;
  }
}
