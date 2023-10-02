// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/md_select.css.js';
import './icons.html.js';

import {AnchorAlignment, CrActionMenuElement, ShowAtPositionConfig} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert_ts.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
    moreOptionsMenu: CrActionMenuElement,
    voiceSelectionMenu: CrActionMenuElement,
    fontTemplate: DomRepeat,
  };
}

interface VoiceDropdown {
  voice: SpeechSynthesisVoice;
  selected: boolean;
  previewPlaying: boolean;
}

interface MenuStateItem<T> {
  title: string;
  icon: string;
  data: T;
  callback: () => void;
}

interface MenuButton {
  id: string;
  icon: string;
  // This is a function instead of just the menu itself because the menu isn't
  // ready by the time we create the MenuButton entries.
  menuToOpen: () => CrActionMenuElement;
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
      textStyleOptions_: Array,
    };
  }

  // This function has to be static because it's called from the ResizeObserver
  // callback which doesn't have access to "this"
  static maybeUpdateMoreOptions(toolbar: HTMLElement) {
    // Hide the more options button first to calculate if we need it
    const moreOptionsButton = toolbar.querySelector('#more') as HTMLElement;
    assert(moreOptionsButton);
    ReadAnythingToolbar.hideElement(moreOptionsButton, false);
    const moreOptionsMenu = toolbar.querySelector('#moreOptionsMenu');
    assert(moreOptionsMenu);
    Array.from(moreOptionsMenu.children).forEach(child => {
      ReadAnythingToolbar.hideElement(child as HTMLElement, false);
    });

    // Show all the buttons before deciding which ones to hide
    const buttons = toolbar.querySelectorAll('.toolbar-button');
    assert(buttons);
    buttons.forEach(btn => {
      ReadAnythingToolbar.showElement(btn as HTMLElement);
    });

    // When scroll width and client width are the different, then the content
    // has overflowed
    if (toolbar.scrollWidth !== toolbar.clientWidth) {
      // If x buttons are pushed off screen, put the more options button before
      // the last x + 1 buttons since adding it will push another button off
      // screen
      for (let i = buttons.length - 1; i >= 0; i--) {
        // Hide the overflowed button in case it's still partially on screen
        const button = buttons[i] as HTMLElement;
        ReadAnythingToolbar.hideElement(button, true);
        // Show the button that was pushed off screen in the more options menu
        const styleButtonInMoreOptions =
            moreOptionsMenu.querySelector('#' + button.id) as HTMLElement;
        if (!styleButtonInMoreOptions) {
          break;
        }
        ReadAnythingToolbar.showElement(styleButtonInMoreOptions);
        if (button.getBoundingClientRect().right < toolbar.clientWidth) {
          ReadAnythingToolbar.showElement(moreOptionsButton);
          toolbar.insertBefore(moreOptionsButton, button);
          break;
        }
      }
    }
  }

  static hideElement(element: HTMLElement, keepSpace: boolean) {
    if (keepSpace) {
      element.style.visibility = 'hidden';
    } else {
      element.style.display = 'none';
    }
  }

  static showElement(element: HTMLElement) {
    element.style.visibility = 'visible';
    element.style.display = 'inline-block';
  }

  // If you change these fonts, please also update read_anything_constants.h
  private fontOptions_: string[] = [];

  private letterSpacingOptions_: Array<MenuStateItem<number>> = [
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

  private lineSpacingOptions_: Array<MenuStateItem<number>> = [
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

  private colorOptions_: Array<MenuStateItem<string>> = [
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

  private voiceSelectionOptions_: Array<MenuStateItem<VoiceDropdown>> = [];

  private rateOptions_: number[] = [0.5, 0.8, 1, 1.2, 1.5, 2, 3, 4];

  private textStyleOptions_: MenuButton[] = [
    {
      id: 'color',
      icon: 'read-anything:color',
      menuToOpen: () => this.$.colorMenu,
    },
    {
      id: 'line-spacing',
      icon: 'read-anything:line-spacing',
      menuToOpen: () => this.$.lineSpacingMenu,
    },
    {
      id: 'letter-spacing',
      icon: 'read-anything:letter-spacing',
      menuToOpen: () => this.$.letterSpacingMenu,
    },
  ];

  private showAtPositionConfig_: ShowAtPositionConfig = {
    top: 20,
    left: 8,
    anchorAlignmentY: AnchorAlignment.AFTER_END,
  };

  private isReadAloudEnabled_: boolean;
  private isHighlightOn_: boolean = true;

  // If Read Aloud is in the paused state.
  private isPaused_: boolean = true;

  override connectedCallback() {
    super.connectedCallback();
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    if (this.isReadAloudEnabled_) {
      this.textStyleOptions_.unshift(
          {
            id: 'font-size',
            icon: 'read-anything:font-size',
            menuToOpen: () => this.$.fontSizeMenu,
          },
          {
            id: 'font',
            icon: 'read-anything:font',
            menuToOpen: () => this.$.fontMenu,
          },
      );

      const shadowRoot = this.shadowRoot;
      assert(shadowRoot);
      const toolbar = shadowRoot.getElementById('toolbar-container');
      assert(toolbar);
      new ResizeObserver(this.onToolbarResize_).observe(toolbar);
    }

    this.updateFonts();
  }

  private onToolbarResize_(entries: ResizeObserverEntry[]) {
    assert(entries.length === 1);
    const toolbar = entries[0].target as HTMLElement;
    ReadAnythingToolbar.maybeUpdateMoreOptions(toolbar);
  }

  private setFontForFontOptions_() {
    let fontOptions: Element[];
    if (this.isReadAloudEnabled_) {
      fontOptions = Array.from(this.$.fontMenu.children);
    } else {
      const shadowRoot = this.shadowRoot;
      assert(shadowRoot);
      const select =
          shadowRoot.getElementById('font-select') as HTMLSelectElement;
      assert(select);
      fontOptions = Array.from(select.options);
    }

    fontOptions.forEach(element => {
      assert(element instanceof HTMLElement);
      if (!element.innerText) {
        return;
      }
      // Update the font of each button to be the same as the font text.
      element.style.fontFamily = element.innerText;
    });
  }

  restoreSettingsFromPrefs(colorSuffix?: string) {
    this.setFontForFontOptions_();

    if (this.isReadAloudEnabled_) {
      const speechRate = parseFloat(chrome.readingMode.speechRate.toFixed(1));
      this.setRateIcon_(speechRate);
      this.setCheckMarkForMenu_(
          this.$.rateMenu, this.rateOptions_.indexOf(speechRate));

      this.setHighlightState_(
          chrome.readingMode.highlightGranularity ===
          chrome.readingMode.highlightOn);
    }
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

  private getIndexOfSetting_(
      menuArray: Array<MenuStateItem<any>>, dataToFind: any): number {
    return menuArray.findIndex((item) => (item.data === dataToFind));
  }

  updateFonts() {
    const fonts = chrome.readingMode.supportedFonts;
    this.fontOptions_ = [];
    fonts.forEach(element => {
      this.fontOptions_.push(element);
    });

    this.$.fontTemplate.render();
  }

  updateUiForPlaying() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('play-pause');
    assert(button);
    button.setAttribute('iron-icon', 'read-anything-20:pause');
    this.isPaused_ = false;

    this.updateStyles({
      '--audio-controls-background': 'var(--color-sys-tonal-container)',
    });

    const toolbar = shadowRoot.getElementById('toolbar-container');
    assert(toolbar);
    ReadAnythingToolbar.maybeUpdateMoreOptions(toolbar);
  }

  showVoicePreviewPlaying(voice: SpeechSynthesisVoice|null) {
    if (!voice) {
      return;
    }
    this.voiceSelectionOptions_ = this.voiceSelectionOptions_.map(
        ({data, ...rest}) => ({
          ...rest,
          data: {
            voice: data.voice,
            selected: data.selected,
            previewPlaying: this.voicesAreEqual_(data.voice, voice),
          },
        }));
  }

  showVoicePreviewDone() {
    this.voiceSelectionOptions_ =
        this.voiceSelectionOptions_.map(({data, ...rest}) => ({
                                          ...rest,
                                          data: {
                                            voice: data.voice,
                                            selected: data.selected,
                                            previewPlaying: false,
                                          },
                                        }));
  }

  updateUiForPausing() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('play-pause');
    assert(button);
    button.setAttribute('iron-icon', 'read-anything-20:play');
    this.isPaused_ = true;

    this.updateStyles({
      '--audio-controls-background': 'transparent',
    });

    const toolbar = shadowRoot.getElementById('toolbar-container');
    assert(toolbar);
    ReadAnythingToolbar.maybeUpdateMoreOptions(toolbar);
  }

  private closeMenus_() {
    this.$.rateMenu.close();
    this.$.colorMenu.close();
    this.$.lineSpacingMenu.close();
    this.$.letterSpacingMenu.close();
    this.$.fontMenu.close();
  }

  private onNextGranularityClick_() {
    if (this.contentPage) {
      this.contentPage.playNextGranularity();
    }
  }

  private onPreviousGranularityClick_() {
    if (this.contentPage) {
      this.contentPage.playPreviousGranularity();
    }
  }

  private onTextStyleMenuButtonClick_(event: DomRepeatEvent<MenuButton>) {
    this.openMenu_(event.model.item.menuToOpen(), event.target as HTMLElement);
  }

  private onShowRateMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.rateMenu, event.target as HTMLElement);
  }

  private voicesAreEqual_(
      voice1?: SpeechSynthesisVoice, voice2?: SpeechSynthesisVoice): boolean {
    if (!voice1 || !voice2) {
      return false;
    }
    return voice1.default === voice2.default && voice1.lang === voice2.lang &&
        voice1.localService === voice2.localService &&
        voice1.name === voice2.name && voice1.voiceURI === voice2.voiceURI;
  }

  // TODO(crbug.com/1474951): Add unit tests
  private onVoiceSelectionMenuClick_(event: MouseEvent) {
    if (this.contentPage) {
      const voices = this.contentPage.getVoices();
      const selectedVoice = this.contentPage.getSpeechSynthesisVoice();

      this.voiceSelectionOptions_ = Object.entries(voices).reduce(
          (aggregateVoiceList: Array<MenuStateItem<VoiceDropdown>>,
           [_, voiceListForLang]) =>
              ([
                ...aggregateVoiceList,
                ...(voiceListForLang).map(speechSynthesisVoice => ({
                                            title: speechSynthesisVoice.name,
                                            icon: '',
                                            data: {
                                              voice: speechSynthesisVoice,
                                              selected: this.voicesAreEqual_(
                                                  selectedVoice,
                                                  speechSynthesisVoice),
                                              previewPlaying: false,
                                            },
                                            callback: () => {},
                                          })),
              ]),
          []);

      this.openMenu_(this.$.voiceSelectionMenu, event.target as HTMLElement);
    }
  }

  private onMoreOptionsClick_(event: MouseEvent) {
    this.openMenu_(this.$.moreOptionsMenu, event.target as HTMLElement);
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
      chrome.readingMode.turnedHighlightOff();
    } else {
      chrome.readingMode.turnedHighlightOn();
    }
    this.setHighlightState_(!this.isHighlightOn_);
  }

  private setHighlightState_(turnOn: boolean) {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const button = shadowRoot.getElementById('highlight');
    assert(button);
    this.isHighlightOn_ = turnOn;
    if (this.isHighlightOn_) {
      button.setAttribute('iron-icon', 'read-anything:highlight-on');
      button.setAttribute('title', loadTimeData.getString('turnHighlightOff'));
    } else {
      button.setAttribute('iron-icon', 'read-anything:highlight-off');
      button.setAttribute('title', loadTimeData.getString('turnHighlightOn'));
    }

    if (this.contentPage) {
      this.contentPage.updateHighlight(this.isHighlightOn_);
    }
  }

  private onLetterSpacingClick_(event: DomRepeatEvent<MenuStateItem<number>>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
        this.$.letterSpacingMenu,
        ReadAnythingElement.prototype.updateLetterSpacing);
  }

  private onLineSpacingClick_(event: DomRepeatEvent<MenuStateItem<number>>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
        this.$.lineSpacingMenu,
        ReadAnythingElement.prototype.updateLineSpacing);
  }

  private onColorClick_(event: DomRepeatEvent<MenuStateItem<string>>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.THEME_CHANGE, this.$.colorMenu,
        ReadAnythingElement.prototype.updateThemeFromWebUi);
  }

  private onVoiceSelectClick_(
      event: DomRepeatEvent<MenuStateItem<VoiceDropdown>>) {
    // TODO(crbug.com/1474951): Save voice to prefs.
    if (this.contentPage) {
      const selectedVoice = event.model.item.data.voice;
      this.contentPage.setSpeechSynthesisVoice(selectedVoice);
      this.voiceSelectionOptions_ = this.voiceSelectionOptions_.map(
          ({data, ...rest}) => ({
            ...rest,
            data: {
              voice: data.voice,
              selected: this.voicesAreEqual_(selectedVoice, data.voice),
              previewPlaying: false,
            },
          }));
    }
  }

  private onVoicePreviewClick_(
      event: DomRepeatEvent<MenuStateItem<VoiceDropdown>>) {
    // Because the preview button is layered onto the voice-selection button,
    // the onVoiceSelectClick_() listener is also subscribed to this event. This
    // line is to make sure that the voice-selection callback is not triggered.
    event.stopImmediatePropagation();

    if (this.contentPage) {
      this.contentPage.previewSpeechSynthesisVoice(event.model.item.data.voice);
    }
  }

  private onTextStyleClick_(
      event: DomRepeatEvent<MenuStateItem<any>>,
      logVal: ReadAnythingSettingsChange, menuClicked: CrActionMenuElement,
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

  private onFontSelectValueChange_(event: Event) {
    const fontName = (event.target as HTMLSelectElement).value;
    chrome.readingMode.onFontChange(fontName);
    if (this.contentPage) {
      this.contentPage.updateFont(fontName);
    }
  }

  private onRateClick_(event: DomRepeatEvent<number>) {
    chrome.readingMode.onSpeechRateChange(event.model.item);
    if (this.contentPage) {
      this.contentPage.onSpeechRateChange(event.model.item);
      this.setRateIcon_(event.model.item);
    }
    this.setCheckMarkForMenu_(this.$.rateMenu, event.model.index);

    this.closeMenus_();
  }

  private setRateIcon_(rate: number) {
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
      ReadAnythingToolbar.hideElement(element, true);
    });
    const checkMark = checkMarks[index] as IronIconElement;
    ReadAnythingToolbar.showElement(checkMark);
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
    if (this.isPaused_) {
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
