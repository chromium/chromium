// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

export interface FinishOrContinueAppElement {
  $: {
    buttonContainer: HTMLElement,
    seeMoreTipsButton: HTMLElement,
    startBrowsingButton: HTMLElement,
  };
}

export class FinishOrContinueAppElement extends CrLitElement {
  static get is() {
    return 'finish-or-continue-app';
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
    'finish-or-continue-app': FinishOrContinueAppElement;
  }
}

customElements.define(
    FinishOrContinueAppElement.is, FinishOrContinueAppElement);
