// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import './icons.html.js';

import {AnchorAlignment, CrActionMenuElement, ShowAtPositionConfig} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert_ts.js';
import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './read_anything_toolbar.html.js';

export interface ReadAnythingToolbar {
  $: {
    rateMenu: CrActionMenuElement,
    colorSubmenu: CrActionMenuElement,
    lineSpacingSubmenu: CrActionMenuElement,
    letterSpacingSubmenu: CrActionMenuElement,
    fontSubmenu: CrActionMenuElement,
    fontSizeMenu: CrActionMenuElement,
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

// Enum for logging when a text style setting is changed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum ReadAnythingSettingsChange {
  FONT_CHANGE = 0,
  FONT_SIZE_CHANGE = 1,
  THEME_CHANGE = 2,
  LINE_HEIGHT_CHANGE = 3,
  LETTER_SPACING_CHANGE = 4,

  // Must be last.
  COUNT = 5,
}

const SETTINGS_CHANGE_UMA = 'Accessibility.ReadAnything.SettingsChange';


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

      fontOptions_: Array,
      rateOptions_: Array,
    };
  }

  // If you change these fonts, please also update read_anything_constants.h
  private fontOptions_: string[] = [];

  private rateOptions_: number[] = [0.5, 0.8, 1, 1.2, 1.5, 2, 3, 4];

  private showAtPositionConfig_: ShowAtPositionConfig = {
    top: 20,
    left: 8,
    anchorAlignmentY: AnchorAlignment.AFTER_END,
  };

  private isReadAloudEnabled_: boolean;

  // If Read Aloud is in the paused state.
  isPaused = true;

  // This is needed to keep a reference to any dynamically added callbacks so
  // that they can be removed with #removeEventListener.
  private elementCallbackMap = new Map<any, () => void>();

  override connectedCallback() {
    super.connectedCallback();
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;

    // TODO(b/1465029): Use the brower's preferred language to hide unsupported
    // fonts.
    this.fontOptions_.push(
        'Poppins',
        'Sans-serif',
        'Serif',
        'Comic Neue',
        'Lexend Deca',
        'EB Garamond',
        'STIX Two Text',
    );

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

      chrome.metricsPrivate.recordEnumerationValue(
          SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
          ReadAnythingSettingsChange.COUNT);
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

      chrome.metricsPrivate.recordEnumerationValue(
          SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
          ReadAnythingSettingsChange.COUNT);
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

      chrome.metricsPrivate.recordEnumerationValue(
          SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.THEME_CHANGE,
          ReadAnythingSettingsChange.COUNT);
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
    this.removeOnClickListeners(this.$.fontSubmenu);
    this.removeOnClickListeners(this.$.lineSpacingSubmenu);
    this.removeOnClickListeners(this.$.colorSubmenu);
    this.removeOnClickListeners(this.$.letterSpacingSubmenu);
  }

  restoreSettingsFromPrefs() {
    const fontNodes = Array.from(this.$.fontSubmenu.children);
    fontNodes.forEach((element) => {
      if (element instanceof HTMLButtonElement) {
        if (!element.innerText) {
          return;
        }
        // Update the font of each button to be the same as the font text.
        element.style.fontFamily = element.innerText;
      }
    });

    this.setCheckMarkForMenu_(
        this.$.fontSubmenu,
        this.fontOptions_.indexOf(chrome.readingMode.fontName));
  }

  updateUiForPlaying() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('play-pause');
    assert(button);
    button.setAttribute('iron-icon', 'read-anything-20:pause');
    this.isPaused = false;
  }

  updateUiForPausing() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('play-pause');
    assert(button);
    button.setAttribute('iron-icon', 'read-anything-20:play');
    this.isPaused = true;
  }

  private removeOnClickListeners(menu: CrActionMenuElement) {
    const nodes = Array.from(menu.children);
    nodes.forEach((element) => {
      if ((element instanceof HTMLButtonElement) &&
          !element.classList.contains('back') &&
          (element.className === 'dropdown-item')) {
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
    this.$.rateMenu.close();
    this.$.colorSubmenu.close();
    this.$.lineSpacingSubmenu.close();
    this.$.letterSpacingSubmenu.close();
    this.$.fontSubmenu.close();
  }

  private onShowRateMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.rateMenu, event.target as HTMLElement);
  }

  private onShowColorSubMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.colorSubmenu, event.target as HTMLElement);
  }

  private onShowLineSpacingSubMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.lineSpacingSubmenu, event.target as HTMLElement);
  }

  private onShowLetterSpacingSubMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.letterSpacingSubmenu, event.target as HTMLElement);
  }

  private onShowFontSubMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.fontSubmenu, event.target as HTMLElement);
  }

  private onShowFontSizeMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.fontSizeMenu, event.target as HTMLElement);
  }

  private openMenu_(menuToOpen: CrActionMenuElement, target: HTMLElement) {
    this.closeMenus_();

    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    menuToOpen.showAt(target, {
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      noOffset: true,
    });
  }

  private onFontClick_(event: DomRepeatEvent<string>) {
    chrome.metricsPrivate.recordEnumerationValue(
        SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.FONT_CHANGE,
        ReadAnythingSettingsChange.COUNT);
    const fontName = event.model.item;
    chrome.readingMode.onFontChange(fontName);
    if (this.contentPage) {
      this.contentPage.updateFont(fontName);
    }
    this.setCheckMarkForMenu_(this.$.fontSubmenu, event.model.index);

    this.closeMenus_();
  }

  private onRateClick_(event: DomRepeatEvent<number>) {
    if (this.contentPage) {
      this.contentPage.onSpeechRateChange(event.model.item);
      this.setRateIcon(event.model.item);
    }

    this.closeMenus_();
  }

  setRateIcon(rate: number) {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('rate');
    assert(button);
    button.setAttribute('iron-icon', 'voice-rate:' + rate);
  }

  private setCheckMarkForMenu_(menu: CrActionMenuElement, index: number) {
    const checkMarks = Array.from(menu.getElementsByClassName('check-mark'));
    assert((index < checkMarks.length) && (index >= 0));
    checkMarks.forEach((element) => {
      assert(element instanceof HTMLElement);
      // TODO(crbug.com/1465029): Ensure this works with screen readers
      element.style.visibility = 'hidden';
    });
    const checkMark = checkMarks[index] as IronIconElement;
    checkMark.style.visibility = 'visible';
  }

  private onFontSizeIncreaseClick_() {
    this.updateFontSize_(true);
  }

  private onFontSizeDecreaseClick_() {
    this.updateFontSize_(false);
  }

  private updateFontSize_(increase: boolean) {
    chrome.metricsPrivate.recordEnumerationValue(
        SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.FONT_SIZE_CHANGE,
        ReadAnythingSettingsChange.COUNT);
    chrome.readingMode.onFontSizeChanged(increase);
    if (this.contentPage) {
      this.contentPage.updateFontSize();
    }
    // Don't close the menu
  }

  private onFontResetClick_() {
    chrome.metricsPrivate.recordEnumerationValue(
        SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.FONT_SIZE_CHANGE,
        ReadAnythingSettingsChange.COUNT);
    chrome.readingMode.onFontSizeReset();
    if (this.contentPage) {
      this.contentPage.updateFontSize();
    }
  }

  private onPlayPauseClick_() {
    if (this.isPaused) {
      this.updateUiForPlaying();
      if (this.contentPage) {
        this.contentPage.playSpeech();
      }
    } else {
      this.updateUiForPausing();
      if (this.contentPage) {
        this.contentPage.stopSpeech();
      }
    }
  }
}

customElements.define('read-anything-toolbar', ReadAnythingToolbar);
