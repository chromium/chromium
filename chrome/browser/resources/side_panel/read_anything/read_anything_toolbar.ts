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
import {loadTimeData} from '//resources/js/load_time_data.js';
import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadAnythingElement} from './app.js';
import {getTemplate} from './read_anything_toolbar.html.js';

export interface ReadAnythingToolbar {
  $: {
    rateMenu: CrActionMenuElement,
    colorMenu: CrActionMenuElement,
    lineSpacingMenu: CrActionMenuElement,
    letterSpacingMenu: CrActionMenuElement,
    fontMenu: CrActionMenuElement,
    fontSizeMenu: CrActionMenuElement,
  };
}

interface MenuStateItem {
  title: string;
  icon: string;
  data: any;
  callback: () => void;
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
      fontOptions_: Array,
      letterSpacingOptions_: Array,
      lineSpacingOptions_: Array,
      colorOptions_: Array,
      rateOptions_: Array,
    };
  }

  // If you change these fonts, please also update read_anything_constants.h
  private fontOptions_: string[] = [];

  private letterSpacingOptions_: MenuStateItem[] = [
    {
      title: loadTimeData.getString('letterSpacingStandardTitle'),
      icon: 'read-anything:letter-spacing-standard',
      data: chrome.readingMode.getLetterSpacingValue(
          chrome.readingMode.standardLetterSpacing),
      callback: () => chrome.readingMode.onStandardLetterSpacing(),
    },
    {
      title: loadTimeData.getString('letterSpacingWideTitle'),
      icon: 'read-anything:letter-spacing-wide',
      data: chrome.readingMode.getLetterSpacingValue(
          chrome.readingMode.wideLetterSpacing),
      callback: () => chrome.readingMode.onWideLetterSpacing(),
    },
    {
      title: loadTimeData.getString('letterSpacingVeryWideTitle'),
      icon: 'read-anything:letter-spacing-very-wide',
      data: chrome.readingMode.getLetterSpacingValue(
          chrome.readingMode.veryWideLetterSpacing),
      callback: () => chrome.readingMode.onVeryWideLetterSpacing(),
    },
  ];

  private lineSpacingOptions_: MenuStateItem[] = [
    {
      title: loadTimeData.getString('lineSpacingStandardTitle'),
      icon: 'read-anything:line-spacing-standard',
      data: chrome.readingMode.getLineSpacingValue(
          chrome.readingMode.standardLineSpacing),
      callback: () => chrome.readingMode.onStandardLineSpacing(),
    },
    {
      title: loadTimeData.getString('lineSpacingLooseTitle'),
      icon: 'read-anything:line-spacing-loose',
      data: chrome.readingMode.getLineSpacingValue(
          chrome.readingMode.looseLineSpacing),
      callback: () => chrome.readingMode.onLooseLineSpacing(),
    },
    {
      title: loadTimeData.getString('lineSpacingVeryLooseTitle'),
      icon: 'read-anything:line-spacing-very-loose',
      data: chrome.readingMode.getLineSpacingValue(
          chrome.readingMode.veryLooseLineSpacing),
      callback: () => chrome.readingMode.onVeryLooseLineSpacing(),
    },
  ];

  private colorOptions_: MenuStateItem[] = [
    {
      title: loadTimeData.getString('defaultColorTitle'),
      icon: 'read-anything:default-theme',
      data: '',
      callback: () => chrome.readingMode.onDefaultTheme(),
    },
    {
      title: loadTimeData.getString('lightColorTitle'),
      icon: 'read-anything:light-theme',
      data: '-light',
      callback: () => chrome.readingMode.onLightTheme(),
    },
    {
      title: loadTimeData.getString('darkColorTitle'),
      icon: 'read-anything:dark-theme',
      data: '-dark',
      callback: () => chrome.readingMode.onDarkTheme(),
    },
    {
      title: loadTimeData.getString('yellowColorTitle'),
      icon: 'read-anything:yellow-theme',
      data: '-yellow',
      callback: () => chrome.readingMode.onYellowTheme(),
    },
    {
      title: loadTimeData.getString('blueColorTitle'),
      icon: 'read-anything:blue-theme',
      data: '-blue',
      callback: () => chrome.readingMode.onBlueTheme(),
    },
  ];

  private rateOptions_: number[] = [0.5, 0.8, 1, 1.2, 1.5, 2, 3, 4];

  private showAtPositionConfig_: ShowAtPositionConfig = {
    top: 20,
    left: 8,
    anchorAlignmentY: AnchorAlignment.AFTER_END,
  };

  private isReadAloudEnabled_: boolean;
  private isHighlightOn_: boolean = true;

  // If Read Aloud is in the paused state.
  isPaused = true;

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
  }

  restoreSettingsFromPrefs(colorSuffix: string|undefined) {
    const fontNodes = Array.from(this.$.fontMenu.children);
    fontNodes.forEach((element) => {
      if (element instanceof HTMLButtonElement) {
        if (!element.innerText) {
          return;
        }
        // Update the font of each button to be the same as the font text.
        element.style.fontFamily = element.innerText;
      }
    });

    // TODO(crbug.com/1474951): Restore rate checkmark
    this.setCheckMarkForMenu_(
        this.$.fontMenu,
        this.fontOptions_.indexOf(chrome.readingMode.fontName));
    this.setCheckMarkForMenu_(
        this.$.colorMenu,
        this.getIndexOfSetting_(this.colorOptions_, colorSuffix));
    this.setCheckMarkForMenu_(
        this.$.lineSpacingMenu,
        this.getIndexOfSetting_(
            this.lineSpacingOptions_,
            parseFloat(chrome.readingMode.lineSpacing.toFixed(2))));
    this.setCheckMarkForMenu_(
        this.$.letterSpacingMenu,
        this.getIndexOfSetting_(
            this.letterSpacingOptions_,
            parseFloat(chrome.readingMode.letterSpacing.toFixed(2))));
  }

  private getIndexOfSetting_(menuArray: MenuStateItem[], dataToFind: any):
      number {
    return menuArray.findIndex((item) => (item.data === dataToFind));
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

  private closeMenus_() {
    this.$.rateMenu.close();
    this.$.colorMenu.close();
    this.$.lineSpacingMenu.close();
    this.$.letterSpacingMenu.close();
    this.$.fontMenu.close();
  }

  private onShowRateMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.rateMenu, event.target as HTMLElement);
  }

  private onShowColorMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.colorMenu, event.target as HTMLElement);
  }

  private onShowLineSpacingMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.lineSpacingMenu, event.target as HTMLElement);
  }

  private onShowLetterSpacingMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.letterSpacingMenu, event.target as HTMLElement);
  }

  private onShowFontMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.fontMenu, event.target as HTMLElement);
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

  private onHighlightClick_() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('highlight');
    assert(button);
    if (this.isHighlightOn_) {
      this.isHighlightOn_ = false;
      button.setAttribute('iron-icon', 'read-anything:highlight-off');
      button.setAttribute('title', loadTimeData.getString('turnHighlightOn'));
    } else {
      this.isHighlightOn_ = true;
      button.setAttribute('iron-icon', 'read-anything:highlight-on');
      button.setAttribute('title', loadTimeData.getString('turnHighlightOff'));
    }
  }

  private onLetterSpacingClick_(event: DomRepeatEvent<MenuStateItem>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
        this.$.letterSpacingMenu,
        ReadAnythingElement.prototype.updateLetterSpacing);
  }

  private onLineSpacingClick_(event: DomRepeatEvent<MenuStateItem>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
        this.$.lineSpacingMenu,
        ReadAnythingElement.prototype.updateLineSpacing);
  }

  private onColorClick_(event: DomRepeatEvent<MenuStateItem>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.THEME_CHANGE, this.$.colorMenu,
        ReadAnythingElement.prototype.updateThemeFromWebUi);
  }

  private onTextStyleClick_(
      event: DomRepeatEvent<MenuStateItem>, logVal: ReadAnythingSettingsChange,
      menuClicked: CrActionMenuElement,
      contentPageCallback: ((data: any) => void)) {
    event.model.item.callback();
    chrome.metricsPrivate.recordEnumerationValue(
        SETTINGS_CHANGE_UMA, logVal, ReadAnythingSettingsChange.COUNT);
    if (this.contentPage) {
      contentPageCallback.call(this.contentPage, event.model.item.data);
    }
    this.setCheckMarkForMenu_(menuClicked, event.model.index);
    this.closeMenus_();
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
    this.setCheckMarkForMenu_(this.$.fontMenu, event.model.index);

    this.closeMenus_();
  }

  private onRateClick_(event: DomRepeatEvent<number>) {
    if (this.contentPage) {
      this.contentPage.onSpeechRateChange(event.model.item);
      this.setRateIcon(event.model.item);
    }
    this.setCheckMarkForMenu_(this.$.rateMenu, event.model.index);

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
