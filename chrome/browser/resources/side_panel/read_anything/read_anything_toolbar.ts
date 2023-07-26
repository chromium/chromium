// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import './icons.html.js';

import {AnchorAlignment, CrActionMenuElement, ShowAtPositionConfig} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert_ts.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './read_anything_toolbar.html.js';

export interface ReadAnythingToolbar {
  $: {
    menu: CrActionMenuElement,
    colorSubmenu: CrActionMenuElement,
    lineSpacingSubmenu: CrActionMenuElement,
    letterSpacingSubmenu: CrActionMenuElement,
  };
}


const ReadAnythingToolbarBase = WebUiListenerMixin(PolymerElement);
export class ReadAnythingToolbar extends ReadAnythingToolbarBase {
  contentPage = document.querySelector('read-anything-app');
  static get is() {
    return 'read-anything-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private showAtPositionConfig_: ShowAtPositionConfig = {
    top: 20,
    left: 8,
    anchorAlignmentY: AnchorAlignment.AFTER_END,
  };

  private onShowSettingsMenuClick_() {
    this.openMenu_(this.$.menu);
  }

  private onShowColorSubMenuClick_() {
    this.openMenu_(this.$.colorSubmenu);
  }

  private onShowLineSpacingSubMenuClick_() {
    this.openMenu_(this.$.lineSpacingSubmenu);
  }

  private onShowLetterSpacingSubMenuClick_() {
    this.openMenu_(this.$.letterSpacingSubmenu);
  }

  private onBackClick_() {
    this.openMenu_(this.$.menu);
  }

  private openMenu_(menuToOpen: CrActionMenuElement) {
    this.$.menu.close();
    this.$.colorSubmenu.close();
    this.$.lineSpacingSubmenu.close();
    this.$.letterSpacingSubmenu.close();

    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('settings');
    assert(button);
    menuToOpen.showAt(button, this.showAtPositionConfig_);
  }

  // TODO(b/1465029): While there is a toolbar in both WebUI and View,
  // investigate if there's an easy way to keep the line and letter spacing
  // numbers consistent with the numbers defined for View in
  // ReadAnythingAppModel#GetLineSpacingValue.
  private onLineSpacingStandardClick_() {
    if (this.contentPage) {
      this.contentPage.updateLineSpacing('1.35');
    }
  }

  private onLineSpacingLooseClick_() {
    if (this.contentPage) {
      this.contentPage.updateLineSpacing('1.5');
    }
  }

  private onLineSpacingVeryLooseClick_() {
    if (this.contentPage) {
      this.contentPage.updateLineSpacing('2');
    }
  }

  private onLetterSpacingStandardClick_() {
    if (this.contentPage) {
      this.contentPage.updateLetterSpacing('0');
    }
  }

  private onLetterSpacingWideClick_() {
    if (this.contentPage) {
      this.contentPage.updateLetterSpacing('.05');
    }
  }

  private onLetterSpacingVeryWideClick_() {
    if (this.contentPage) {
      this.contentPage.updateLetterSpacing('.1');
    }
  }
}

customElements.define('read-anything-toolbar', ReadAnythingToolbar);
