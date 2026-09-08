// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topics_view.css.js';
import {getHtml} from './topics_view.html.js';

export class TopicsViewElement extends CrLitElement {
  static get is() {
    return 'topics-view';
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
    'topics-view': TopicsViewElement;
  }
}

customElements.define(TopicsViewElement.is, TopicsViewElement);
