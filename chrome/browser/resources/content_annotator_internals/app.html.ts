// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ContentAnnotatorInternalsAppElement} from './app.js';

export function getHtml(this: ContentAnnotatorInternalsAppElement) {
  return html`
<h1>Content Annotator Internals</h1>
<div id="example-div">${this.message_}</div>`;
}
