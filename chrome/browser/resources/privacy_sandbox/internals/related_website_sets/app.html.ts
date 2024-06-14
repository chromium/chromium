// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RelatedWebsiteSetsAppElement} from './app.js';

export function getHtml(this: RelatedWebsiteSetsAppElement) {
  return html`
<related-website-sets-toolbar
  id="toolbar"
  .pageName="${this.pageTitle_}"
  .narrow="${this.narrow_}">
</related-website-sets-toolbar>`;
}