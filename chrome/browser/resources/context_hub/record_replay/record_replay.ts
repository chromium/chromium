// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './record_replay.html.js';

export class RecordReplayElement extends CrLitElement {
  static get is() {
    return 'record-replay';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'record-replay': RecordReplayElement;
  }
}

customElements.define(RecordReplayElement.is, RecordReplayElement);
