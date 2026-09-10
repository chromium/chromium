// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// other imports go here...
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './my_component.css.js';
import {getHtml} from './my_component.html.js';

export interface MyComponentElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class MyComponentElement extends CrLitElement {
  static get is() {
    return 'my-component';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      value_: {type: String},
    };
  }

  protected accessor value_: string = 'Default Mock Value';
}

customElements.define(MyComponentElement.is, MyComponentElement);
