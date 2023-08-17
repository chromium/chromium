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
  DEFAULT_COLOR = 3,
  LIGHT = 4,
  DARK = 5,
  YELLOW = 6,
  BLUE = 7,
  LETTER_STANDARD = 8,
  WIDE = 9,
  VERY_WIDE = 10,
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

    // Configure on-click listeners for letter spacing.
    const onLetterSpacingClick = (element: number) => {
      let data: number|undefined;

      switch (element) {
        case MenuStateValue.LETTER_STANDARD:
          chrome.readingMode.onStandardLetterSpacing();
          data = chrome.readingMode.standardLetterSpacing;
          break;
        case MenuStateValue.WIDE:
          chrome.readingMode.onWideLetterSpacing();
          data = chrome.readingMode.wideLetterSpacing;
          break;
        case MenuStateValue.VERY_WIDE:
          chrome.readingMode.onVeryWideLetterSpacing();
          data = chrome.readingMode.veryWideLetterSpacing;
          break;
        default:
          // Do nothing;
      }

      if (this.contentPage && data) {
        this.contentPage.updateLetterSpacing(
            chrome.readingMode.getLetterSpacingValue(data));
      }
      this.closeMenus_();
    };

    // Configure on-click listeners for theme.
    const onThemeClick = (element: number) => {
      let colorSuffix: string|undefined;

      switch (element) {
        case MenuStateValue.DEFAULT_COLOR:
          chrome.readingMode.onDefaultTheme();
          colorSuffix = '';
          break;
        case MenuStateValue.LIGHT:
          chrome.readingMode.onLightTheme();
          colorSuffix = '-light';
          break;
        case MenuStateValue.DARK:
          chrome.readingMode.onDarkTheme();
          colorSuffix = '-dark';
          break;
        case MenuStateValue.YELLOW:
          chrome.readingMode.onYellowTheme();
          colorSuffix = '-yellow';
          break;
        case MenuStateValue.BLUE:
          chrome.readingMode.onBlueTheme();
          colorSuffix = '-blue';
          break;
        default:
          // Do nothing;
      }

      if (this.contentPage && (colorSuffix !== undefined)) {
        this.contentPage.updateThemeFromWebUi(colorSuffix);
      }
      this.closeMenus_();
    };
    this.addOnClickListeners(this.$.lineSpacingSubmenu, onLineSpacingClick);
    this.addOnClickListeners(this.$.colorSubmenu, onThemeClick);
    this.addOnClickListeners(this.$.letterSpacingSubmenu, onLetterSpacingClick);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeOnClickListeners(this.$.lineSpacingSubmenu);
    this.removeOnClickListeners(this.$.colorSubmenu);
    this.removeOnClickListeners(this.$.letterSpacingSubmenu);
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
