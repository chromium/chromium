// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ToolbarAppElement} from './app.js';

export function getHtml(this: ToolbarAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <link rel="stylesheet"
   href="layout_constants_v${this.navigationControlsState_.layoutConstantsVersion}.css">
${this.isBackForwardButtonEnabled_ ? html`
  <back-forward-button id="back" direction="back"
   .state="${this.navigationControlsState_.backForwardControlState.backButtonState}"
   .leadingMargin="${this.navigationControlsState_.backForwardControlState.backButtonLeadingMargin}">
  </back-forward-button>
  <back-forward-button id="forward" direction="forward"
   .state="${this.navigationControlsState_.backForwardControlState.forwardButtonState}"
   .hidden="${!this.navigationControlsState_.backForwardControlState.forwardButtonState.shouldBeShown}">
  </back-forward-button>` : ''}
  ${this.isReloadButtonEnabled_ ? html`
    <reload-button id="reload"
      .state="${this.navigationControlsState_.reloadControlState}">
    </reload-button>
  ` : ''}
  ${this.isHomeButtonEnabled_ ? html`
    <home-button id="home"
      .state="${this.navigationControlsState_.homeControlState}"
      .hidden="${!this.navigationControlsState_.homeControlState.shouldBeShown}">
    </home-button>
  ` : ''}
  ${this.isSplitTabsButtonEnabled_ ? html`
    <split-tabs-button id="split-tabs"
        .state="${this.navigationControlsState_.splitTabsControlState}"
        .hidden="${!this.navigationControlsState_.splitTabsControlState.isPinned &&
                   !this.navigationControlsState_.splitTabsControlState.isCurrentTabSplit}">
    </split-tabs-button>
  ` : ''}
  ${this.isLocationBarEnabled_ ? html`
    <location-bar id="location-bar"
        .locationBarState="${this.navigationControlsState_.locationBarState}">
    </location-bar>
  ` : ''}
  ${this.isPinnedToolbarActionsEnabled_ ? html`
    <pinned-toolbar-actions id="pinnedToolbarActions"
        .state="${this.navigationControlsState_.pinnedToolbarActionsState}">
    </pinned-toolbar-actions>
  ` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
