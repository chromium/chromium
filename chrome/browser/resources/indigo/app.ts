// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './motion_overlay.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export class IndigoImageReplacementAppElement extends CrLitElement {
  static get is() {
    return 'indigo-image-replacement-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showOverlay_: {type: Boolean},
    };
  }

  protected accessor showOverlay_: boolean = false;

  override firstUpdated() {
    this.showOverlay_ = true;
  }

  protected onMotionComplete_() {
    this.showOverlay_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indigo-image-replacement-app': IndigoImageReplacementAppElement;
  }
}

customElements.define(
    IndigoImageReplacementAppElement.is, IndigoImageReplacementAppElement);
