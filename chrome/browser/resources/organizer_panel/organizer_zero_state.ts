// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './organizer_zero_state.css.js';
import {getHtml} from './organizer_zero_state.html.js';

export class OrganizerZeroStateElement extends CrLitElement {
  static get is() {
    return 'organizer-zero-state';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'organizer-zero-state': OrganizerZeroStateElement;
  }
}

customElements.define(OrganizerZeroStateElement.is, OrganizerZeroStateElement);
