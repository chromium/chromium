// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class OrganizerPanelAppElement extends CrLitElement {
  static get is() {
    return 'organizer-panel-app';
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
    'organizer-panel-app': OrganizerPanelAppElement;
  }
}

customElements.define(OrganizerPanelAppElement.is, OrganizerPanelAppElement);
