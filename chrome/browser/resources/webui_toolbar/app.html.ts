// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ToolbarAppElement} from './app.js';

export function getHtml(this: ToolbarAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <link rel="stylesheet"
   href="layout_constants_v${this.toolbarState_.layoutConstantsVersion}.css">
${this.isBackForwardButtonEnabled_ ? html`
  <back-forward-button id="back" direction="back"
   .state="${this.toolbarState_.backForwardControlState.backButtonState}"
   .windowIsMaximizedOrFullscreen="${
       this.toolbarState_.backForwardControlState.windowIsMaximizedOrFullscreen}"
   .touchUi="${this.toolbarState_.touchUi}">
  </back-forward-button>
  <back-forward-button id="forward" direction="forward"
   .state="${this.toolbarState_.backForwardControlState.forwardButtonState}"
   .hidden="${
       !this.toolbarState_.backForwardControlState.forwardButtonState
            .shouldBeShown}"
   .touchUi="${this.toolbarState_.touchUi}">
  </back-forward-button>` : ''}
  ${this.isReloadButtonEnabled_ ? html`
    <reload-button id="reload"
      .state="${this.toolbarState_.reloadControlState}"
      .touchUi="${this.toolbarState_.touchUi}">
    </reload-button>
  ` : ''}
  ${this.isHomeButtonEnabled_ ? html`
    <home-button id="home"
      .state="${this.toolbarState_.homeControlState}"
      .hidden="${!this.toolbarState_.homeControlState.shouldBeShown}"
      .touchUi="${this.toolbarState_.touchUi}">
    </home-button>
  ` : ''}
  ${this.isSplitTabsButtonEnabled_ ? html`
    <split-tabs-button id="split-tabs"
        .state="${this.toolbarState_.splitTabsControlState}"
        .hidden="${!this.toolbarState_.splitTabsControlState.shouldBeShown}">
    </split-tabs-button>
  ` : ''}
  ${this.isLocationBarEnabled_ ? html`
    <location-bar id="location-bar"
        .locationBarState="${this.toolbarState_.locationBarState}"
        .touchUi="${this.toolbarState_.touchUi}">
    </location-bar>
  ` : ''}
  ${this.isExtensionsContainerEnabled_ ? html`
    <webui-toolbar-extensions id="extensions"
        .states="${this.toolbarState_.extensionsState}">
    </webui-toolbar-extensions>
  ` : ''}
  ${this.isPinnedToolbarActionsEnabled_ ? html`
    <pinned-toolbar-actions id="pinnedToolbarActions"
        .states="${this.toolbarState_.pinnedToolbarActionsState}">
    </pinned-toolbar-actions>
  ` : ''}
  ${this.isBatterySaverButtonEnabled_ ? html`
    <battery-saver-button id="battery-saver"
        .state="${this.toolbarState_.batterySaverControlState}"
        .hidden="${!this.toolbarState_.batterySaverControlState.shouldBeShown}">
    </battery-saver-button>
  ` : ''}
  ${this.isPerformanceInterventionButtonEnabled_ ? html`
    <performance-intervention-button id="performance-intervention"
      .state="${
        this.toolbarState_.performanceInterventionControlState}"
      .hidden="${
        !this.toolbarState_.performanceInterventionControlState
            .shouldBeShown}">
    </performance-intervention-button>
  ` : ''}
  <if expr="is_win or is_macosx or is_linux">
  ${this.isMediaButtonEnabled_ ? html`
    <media-button id="media"
        .state="${this.toolbarState_.mediaControlState}"
        .hidden="${!this.toolbarState_.mediaControlState.shouldBeShown}">
    </media-button>
  ` : ''}
  </if>
  ${this.isGlicButtonEnabled_ ? html`
    <glic-button id="glic-button"
        .state="${this.toolbarState_.glicButtonState}"
        .hidden="${!this.toolbarState_.glicButtonState.shouldShow}">
    </glic-button>
  ` : ''}
  ${this.isAvatarButtonEnabled_ ? html`
    <avatar-button id="avatar"
        .state="${this.toolbarState_.avatarControlState}">
    </avatar-button>
  ` : ''}
  ${this.webUIToolbarFullyEnabled_ ? html`
    <overflow-button id="overflow" hidden
        .getOverflowedMenuItems="${() => this.getOverflowedMenuItems()}"
        .state="${this.toolbarState_.overflowButtonControlState}">
    </overflow-button>
  ` : ''}
  ${this.isAppMenuButtonEnabled_ ? html`
    <app-menu-button id="app-menu"
        .state="${this.toolbarState_.appMenuControlState}">
    </app-menu-button>
  ` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
