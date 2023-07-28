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
    fontSubmenu: CrActionMenuElement,
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

  override connectedCallback() {
    super.connectedCallback();

    const onFontClick = (fontName: string) => {
      this.onFontClick_(fontName);
    };

    const fontNodes = Array.from(this.$.fontSubmenu.children);
    fontNodes.forEach((element) => {
      if (element instanceof HTMLButtonElement) {
        if (element.classList.contains('back') || !element.innerText) {
          return;
        }
        // Update the font of each button to be the same as the font text.
        element.style.fontFamily = element.innerText;
        // Set the onclick listener for each button so that the content
        // page font updates when a button is clicked.
        element.addEventListener('click', function() {
          onFontClick(element.innerText);
        });
      }
    });
  }

  private onDefaultTheme_() {
    this.updateTheme_('');
  }

  private onLightTheme_() {
    this.updateTheme_('-light');
  }

  private onDarkTheme_() {
    this.updateTheme_('-dark');
  }

  private onBlueTheme_() {
    this.updateTheme_('-blue');
  }

  private onYellowTheme_() {
    this.updateTheme_('-yellow');
  }

  private updateTheme_(colorSuffix: string) {
    if (this.contentPage) {
      this.contentPage.updateThemeFromWebUi(colorSuffix);
    }
    this.closeMenus_();
  }

  private closeMenus_() {
    this.$.menu.close();
    this.$.colorSubmenu.close();
    this.$.lineSpacingSubmenu.close();
    this.$.letterSpacingSubmenu.close();
    this.$.fontSubmenu.close();
  }

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

  private onShowFontSubMenuClick_() {
    this.openMenu_(this.$.fontSubmenu);
  }

  private openMenu_(menuToOpen: CrActionMenuElement) {
    this.closeMenus_();

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
    this.onLineSpacingClick_('1.35');
  }

  private onLineSpacingLooseClick_() {
    this.onLineSpacingClick_('1.5');
  }

  private onLineSpacingVeryLooseClick_() {
    this.onLineSpacingClick_('2');
  }

  private onLineSpacingClick_(lineSpacing: string) {
    if (this.contentPage) {
      this.contentPage.updateLineSpacing(lineSpacing);
    }

    this.closeMenus_();
  }

  private onLetterSpacingStandardClick_() {
    this.onLetterSpacingClick_('0');
  }

  private onLetterSpacingWideClick_() {
    this.onLetterSpacingClick_('.05');
  }

  private onLetterSpacingVeryWideClick_() {
    this.onLetterSpacingClick_('.1');
  }

  private onLetterSpacingClick_(letterSpacing: string) {
    if (this.contentPage) {
      this.contentPage.updateLetterSpacing(letterSpacing);
    }
    this.closeMenus_();
  }

  private onFontClick_(fontName: string) {
    if (this.contentPage) {
      this.contentPage.updateFont(fontName);
    }

    this.closeMenus_();
  }
}

customElements.define('read-anything-toolbar', ReadAnythingToolbar);
