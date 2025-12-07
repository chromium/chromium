// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from '../demo.css.js';

import {getHtml} from './cr_chip_demo.html.js';

export class CrChipDemoElement extends CrLitElement {
  static get is() {
    return 'cr-chip-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

export const tagName = CrChipDemoElement.is;

customElements.define(CrChipDemoElement.is, CrChipDemoElement);
