// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {getTemplate} from './page_displayer.html.js';

export class PageDisplayerElement extends PolymerElement {
  static get is() {
    return 'page-displayer' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      active: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      section: {
        type: Number,
        reflectToAttribute: true,
      },
    };
  }

  active: boolean;
  section: Section;

  override ready() {
    super.ready();

    assert(this.section in Section, `Invalid section: ${this.section}.`);
  }

  override focus() {
    this.shadowRoot!.getElementById('focusHost')!.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PageDisplayerElement.is]: PageDisplayerElement;
  }
}

customElements.define(PageDisplayerElement.is, PageDisplayerElement);
