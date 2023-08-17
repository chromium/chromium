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

enum MenuStateValue {
  LINE_STANDARD = 0,
  LOOSE = 1,
  VERY_LOOSE = 2,
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
    return {
      menuStateEnum_: {
        type: Object,
        value: MenuStateValue,
      },
    };
  }

  private showAtPositionConfig_: ShowAtPositionConfig = {
    top: 20,
    left: 8,
    anchorAlignmentY: AnchorAlignment.AFTER_END,
  };

  // This is needed to keep a reference to any dynamically added callbacks so
  // that they can be removed with #removeEventListener.
  private elementCallbackMap = new Map<any, () => void>();

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

    // Configure on-click listeners for line spacing.
    const onLineSpacingClick = (element: number) => {
      let data: number|undefined;

      switch (element) {
        case MenuStateValue.LINE_STANDARD:
          chrome.readingMode.onStandardLineSpacing();
          data = chrome.readingMode.standardLineSpacing;
          break;
        case MenuStateValue.LOOSE:
          chrome.readingMode.onLooseLineSpacing();
          data = chrome.readingMode.looseLineSpacing;
          break;
        case MenuStateValue.VERY_LOOSE:
          chrome.readingMode.onVeryLooseLineSpacing();
          data = chrome.readingMode.veryLooseLineSpacing;
          break;
        default:
          // Do nothing;
      }

      if (this.contentPage && data) {
        this.contentPage.updateLineSpacing(
            chrome.readingMode.getLineSpacingValue(data));
      }
      this.closeMenus_();
    };
    this.addOnClickListeners(this.$.lineSpacingSubmenu, onLineSpacingClick);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeOnClickListeners(this.$.lineSpacingSubmenu);
  }

  private removeOnClickListeners(menu: CrActionMenuElement) {
    const nodes = Array.from(menu.children);
    nodes.forEach((element) => {
      if ((element instanceof HTMLButtonElement) &&
          !element.classList.contains('back') && element.hasAttribute('data')) {
        const callback = this.elementCallbackMap.get(element);
        if (callback) {
          element.removeEventListener('click', callback);
        }
        this.elementCallbackMap.delete(element);
      }
    });
  }

  private addOnClickListeners(
      menu: CrActionMenuElement,
      onMenuElementClick: (element: number) => void) {
    const nodes = Array.from(menu.children);
    nodes.forEach((element) => {
      if ((element instanceof HTMLButtonElement) &&
          !element.classList.contains('back') && element.hasAttribute('data')) {
        const callback = () => {
          onMenuElementClick(parseInt(element.getAttribute('data')!));
        };
        this.elementCallbackMap.set(element, callback);
        element.addEventListener('click', callback);
      }
    });
  }

  private onDefaultTheme_() {
    chrome.readingMode.onDefaultTheme();
    this.updateTheme_('');
  }

  private onLightTheme_() {
    chrome.readingMode.onLightTheme();
    this.updateTheme_('-light');
  }

  private onDarkTheme_() {
    chrome.readingMode.onDarkTheme();
    this.updateTheme_('-dark');
  }

  private onBlueTheme_() {
    chrome.readingMode.onBlueTheme();
    this.updateTheme_('-blue');
  }

  private onYellowTheme_() {
    chrome.readingMode.onYellowTheme();
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

  private onLetterSpacingStandardClick_() {
    chrome.readingMode.onStandardLetterSpacing();
    this.onLetterSpacingClick_(chrome.readingMode.standardLetterSpacing);
  }

  private onLetterSpacingWideClick_() {
    chrome.readingMode.onWideLetterSpacing();
    this.onLetterSpacingClick_(chrome.readingMode.wideLetterSpacing);
  }

  private onLetterSpacingVeryWideClick_() {
    chrome.readingMode.onVeryWideLetterSpacing();
    this.onLetterSpacingClick_(chrome.readingMode.veryWideLetterSpacing);
  }

  private onLetterSpacingClick_(letterSpacing: number) {
    if (this.contentPage) {
      this.contentPage.updateLetterSpacing(
          chrome.readingMode.getLetterSpacingValue(letterSpacing));
    }
    this.closeMenus_();
  }

  private onFontClick_(fontName: string) {
    chrome.readingMode.onFontChange(fontName);
    if (this.contentPage) {
      this.contentPage.updateFont(fontName);
    }

    this.closeMenus_();
  }

  private onFontSizeIncreaseClick_() {
    this.updateFontSize_(true);
  }

  private onFontSizeDecreaseClick_() {
    this.updateFontSize_(false);
  }
  private updateFontSize_(increase: boolean) {
    if (this.contentPage) {
      this.contentPage.updateFontSize(increase);
    }
    // Don't close the menu
  }
}

customElements.define('read-anything-toolbar', ReadAnythingToolbar);
