// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerZoomToolbarElement} from './viewer_zoom_toolbar.js';

export function getHtml(this: ViewerZoomToolbarElement) {
  return html`<!--_html_template_start_-->
<div id="zoom-buttons">
  <viewer-zoom-button id="fitButton" @fabclick="${this.fitToggle}"
      tooltips="$i18n{tooltipFitToPage},$i18n{tooltipFitToWidth}"
      .keyboardNavigationActive="${this.keyboardNavigationActive_}"
      icons="${this.iconsetName_()}:fullscreen-exit cr:fullscreen">
  </viewer-zoom-button>
  <viewer-zoom-button id="zoom-in-button"
      icons="${this.iconsetName_()}:add"
      tooltips="$i18n{tooltipZoomIn}"
      .keyboardNavigationActive="${this.keyboardNavigationActive_}"
      @fabclick="${this.zoomIn}"></viewer-zoom-button>
  <viewer-zoom-button id="zoom-out-button"
      icons="${this.iconsetName_()}:remove"
      tooltips="$i18n{tooltipZoomOut}"
      .keyboardNavigationActive="${this.keyboardNavigationActive_}"
      @fabclick="${this.zoomOut}"></viewer-zoom-button>
</div>
<!--_html_template_end_-->`;
}
