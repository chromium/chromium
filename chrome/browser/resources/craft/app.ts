// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class CraftAppElement extends CrLitElement {
  static get is() {
    return 'craft-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    ColorChangeUpdater.forDocument().start();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'craft-app': CraftAppElement;
  }
}

customElements.define(CraftAppElement.is, CraftAppElement);
