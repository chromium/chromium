// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrUrlListItemDemoElement} from './cr_url_list_item_demo.js';

export function getHtml(this: CrUrlListItemDemoElement) {
  return html`
<h1>cr-url-list-item</h1>
<div class="demos">
  <cr-url-list-item count="23" title="A Bookmark Folder"
      description="23 bookmarks">
  </cr-url-list-item>

  <cr-url-list-item count="23" always-show-suffix
      title="A folder with the X icon always visible"
      description="23 bookmarks">
    <cr-icon-button iron-icon="cr:close" slot="suffix">
    </cr-icon-button>
  </cr-url-list-item>

  <cr-url-list-item url="http://www.google.com"
      title="Google (as anchor)"
      description="google.com"
      description-meta="3 days ago" as-anchor>
    <cr-icon-button iron-icon="cr:check-circle" slot="suffix">
    </cr-icon-button>
    <cr-icon-button iron-icon="cr:more-vert" slot="suffix">
    </cr-icon-button>
  </cr-url-list-item>

  <cr-url-list-item url="http://www.google.com"
      title="Google"
      description="google.com"
      description-meta="3 days ago">
    <div slot="suffix">More descriptive text</div>
    <cr-icon-button iron-icon="cr:more-vert" slot="suffix">
    </cr-icon-button>
  </cr-url-list-item>

  <cr-url-list-item url="http://www.google.com"
        title="Google"
        description="google.com">
    <div class="badge" slot="badges">
      <cr-icon icon="cr:error-outline"></cr-icon> Badge 1
    </div>
    <div class="badge" slot="badges">
      <cr-icon icon="cr:insert-drive-file"></cr-icon> Badge 2
    </div>
    <cr-icon-button iron-icon="cr:check-circle" slot="suffix">
    </cr-icon-button>
    <cr-icon-button iron-icon="cr:more-vert" slot="suffix">
    </cr-icon-button>
  </cr-url-list-item>

  <cr-url-list-item url="http://maps.google.com"
      title="A really really really really really really really really really
          really really really really really really really really really really
          really long title"
      description="aurlthatisreallyreallyreallyreallyreallyreallylong.com"
      reverse-elide-description>
  </cr-url-list-item>
</div>

<h2>Compact</h2>
<div class="demos">
  <cr-url-list-item url="http://www.google.com" size="compact"
      title="Google" description="google.com">
    <div class="badge" slot="badges">
      <cr-icon icon="cr:error-outline"></cr-icon> Badge 1
    </div>
    <div class="badge" slot="badges">
      <cr-icon icon="cr:insert-drive-file"></cr-icon> Badge 2
    </div>
  </cr-url-list-item>

  <cr-url-list-item url="http://www.google.com" size="compact"
      title="A really really really really really really really really really
          really really really really really really really really really really
          really long title"
      description="aurlthatisreallyreallyreallyreallyreallyreallylong.com"
      reverse-elide-description
      description-meta="2 hours ago">
  </cr-url-list-item>

  <cr-url-list-item count="23" size="compact"
      title="Bookmark folder" description="23 bookmarks">
    <cr-icon-button iron-icon="cr:more-vert" slot="suffix">
    </cr-icon-button>
  </cr-url-list-item>
</div>

<h2>Large</h2>
<div class="demos">
  <cr-url-list-item url="http://www.google.com" size="large"
      title="Google" description="google.com" description-meta="2 mins">
    <div class="badge" slot="badges">
      <cr-icon icon="cr:error-outline"></cr-icon> Badge 1
    </div>
    <div class="badge" slot="badges">
      <cr-icon icon="cr:insert-drive-file"></cr-icon> Badge 2
    </div>
  </cr-url-list-item>
  <cr-url-list-item count="31" size="large" title="All bookmarks">
  </cr-url-list-item>
</div>

<h2>With other types of content</h2>
<div class="demos">
  <cr-url-list-item count="23" size="compact"
      title="This should not be visible">
    <div slot="content"><cr-input value="Item name"></cr-input></div>
    <cr-icon-button iron-icon="cr:more-vert" slot="suffix" disabled>
    </cr-icon-button>
  </cr-url-list-item>

  <cr-url-list-item url="http://www.google.com" size="compact"
      title="Bookmark folder" description="23 bookmarks">
    <cr-checkbox slot="prefix"></cr-checkbox>
  </cr-url-list-item>
</div>`;
}
