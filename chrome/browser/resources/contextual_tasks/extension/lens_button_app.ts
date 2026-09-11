// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './lens_button_app.css.js';
import {getHtml} from './lens_button_app.html.js';

export interface LensButtonAppElement {
  $: {
    lensButton: CrIconButtonElement,
  };
}

export class LensButtonAppElement extends CrLitElement {
  static get is() {
    return 'lens-button-app';
  }

  static override get styles(): CSSResultGroup {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      active: {
        type: Boolean,
        reflect: true,
      },
      disabled: {
        type: Boolean,
        reflect: true,
      },
      label_: {type: String},
    };
  }

  accessor active: boolean = false;
  accessor disabled: boolean = false;
  protected accessor label_: string = loadTimeData.isInitialized() &&
          loadTimeData.valueExists('lensSearchButtonLabel') ?
      loadTimeData.getString('lensSearchButtonLabel') :
      '';
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-button-app': LensButtonAppElement;
  }
}

customElements.define(LensButtonAppElement.is, LensButtonAppElement);
