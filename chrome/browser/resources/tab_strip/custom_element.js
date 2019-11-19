// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Super class for all custom elements defined in the tab strip.
 */
export class CustomElement extends HTMLElement {
  constructor() {
    super();

    this.attachShadow({mode: 'open'});
    const template = document.createElement('template');
    template.innerHTML = this.constructor.template || '';
    this.shadowRoot.appendChild(template.content.cloneNode(true));
  }
}
