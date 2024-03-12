// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/md_select.css.js';
import './voice_selection_menu.js';
import './icons.html.js';

import type {CrActionMenuElement, ShowAtPositionConfig} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import type {DomRepeat, DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadAnythingElement} from './app.js';
import {validatedFontName} from './common.js';
import {getTemplate} from './read_anything_toolbar.html.js';
import type {VoiceSelectionMenuElement} from './voice_selection_menu.js';

export interface ReadAnythingToolbarElement {
  $: {
    rateMenu: CrActionMenuElement,
    colorMenu: CrActionMenuElement,
    lineSpacingMenu: CrActionMenuElement,
    letterSpacingMenu: CrActionMenuElement,
    fontMenu: CrActionMenuElement,
    fontSizeMenu: CrActionMenuElement,
    moreOptionsMenu: CrActionMenuElement,
    voiceSelectionMenu: VoiceSelectionMenuElement,
    fontTemplate: DomRepeat,
  };
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
  ariaLabel: string;
  // This is a function instead of just the menu itself because the menu isn't
  // ready by the time we create the MenuButton entries.
  menuToOpen: () => CrActionMenuElement;
}

interface ToggleButton {
  id: string;
  icon: string;
  title: string;
  callback: (event: DomRepeatEvent<ToggleButton>) => void;
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
  LINKS_ENABLED_CHANGE = 5,

  // Must be last.
  COUNT = 6,
}

const SETTINGS_CHANGE_UMA = 'Accessibility.ReadAnything.SettingsChange';
const moreOptionsClass = '.more-options-icon';
const activeClass = ' active';

// Link toggle button constants.
export const LINKS_ENABLED_ICON = 'read-anything:links-enabled';
export const LINKS_DISABLED_ICON = 'read-anything:links-disabled';
export const LINK_TOGGLE_BUTTON_ID = 'link-toggle-button';

// Events emitted from the toolbar to the app
export const FONT_SIZE_EVENT = 'font-size-change';
export const FONT_EVENT = 'font-change';
export const RATE_EVENT = 'rate-change';
export const PLAY_PAUSE_EVENT = 'play-pause-click';
export const HIGHLIGHT_TOGGLE_EVENT = 'highlight-toggle';
export const NEXT_GRANULARITY_EVENT = 'next-granularity-click';
export const PREVIOUS_GRANULARITY_EVENT = 'previous-granularity-click';
export const LINKS_EVENT = 'links-toggle';

const ReadAnythingToolbarElementBase = WebUiListenerMixin(PolymerElement);
export class ReadAnythingToolbarElement extends ReadAnythingToolbarElementBase {
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
      textStyleToggles_: Array,
      paused: Boolean,
      hasContent: Boolean,
      selectedVoice: Object,
      availableVoices: Array,
      localeToDisplayName: Object,
      previewVoicePlaying: Object,
    };
  }

  // This function has to be static because it's called from the ResizeObserver
  // callback which doesn't have access to "this"
  static maybeUpdateMoreOptions(toolbar: HTMLElement) {
    // Hide the more options button first to calculate if we need it
    const moreOptionsButton = toolbar.querySelector<HTMLElement>('#more');
    assert(moreOptionsButton);
    ReadAnythingToolbarElement.hideElement(moreOptionsButton, false);

    // Show all the buttons that would go in the overflow menu to see if they
    // fit
    const buttons = Array.from(toolbar.querySelectorAll('.toolbar-button'));
    assert(buttons);
    const moreOptionsButtons = toolbar.querySelectorAll(moreOptionsClass);
    assert(moreOptionsButtons);
    const buttonsOnToolbarToMaybeHide =
        buttons.slice(buttons.length - moreOptionsButtons.length);
    buttonsOnToolbarToMaybeHide.forEach(btn => {
      ReadAnythingToolbarElement.showElement(btn as HTMLElement);
    });

    const parentWidth = toolbar.offsetParent?.clientWidth;
    assert(parentWidth);

    // When the toolbar's width exceeds the parent width, then the content has
    // overflowed.
    if (toolbar.clientWidth > parentWidth) {
      ReadAnythingToolbarElement.showElement(moreOptionsButton);
      // Hide all the buttons on the toolbar that are in the more options menu
      buttonsOnToolbarToMaybeHide.forEach(btn => {
        ReadAnythingToolbarElement.hideElement(btn as HTMLElement, true);
      });
      toolbar.insertBefore(moreOptionsButton, buttonsOnToolbarToMaybeHide[0]);
      (moreOptionsButtons.item(0) as HTMLElement).style.marginLeft = '16px';
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

  private textStyleToggles_: ToggleButton[] = [
    {
      id: LINK_TOGGLE_BUTTON_ID,
      icon: chrome.readingMode.linksEnabled?
      LINKS_ENABLED_ICON: LINKS_DISABLED_ICON,
      title: chrome.readingMode.linksEnabled?
           loadTimeData.getString('disableLinksLabel'):
               loadTimeData.getString('enableLinksLabel'),
      callback: this.onToggleLinksClick_.bind(this),
    },
  ];

  private colorOptions_: Array<MenuStateItem<string>> = [
    {
      title: loadTimeData.getString('defaultColorTitle'),
      icon: 'read-anything-20:default-theme',
      data: '',
      callback: () => chrome.readingMode.onDefaultTheme(),
    },
    {
      title: loadTimeData.getString('lightColorTitle'),
      icon: 'read-anything-20:light-theme',
      data: '-light',
      callback: () => chrome.readingMode.onLightTheme(),
    },
    {
      title: loadTimeData.getString('darkColorTitle'),
      icon: 'read-anything-20:dark-theme',
      data: '-dark',
      callback: () => chrome.readingMode.onDarkTheme(),
    },
    {
      title: loadTimeData.getString('yellowColorTitle'),
      icon: 'read-anything-20:yellow-theme',
      data: '-yellow',
      callback: () => chrome.readingMode.onYellowTheme(),
    },
    {
      title: loadTimeData.getString('blueColorTitle'),
      icon: 'read-anything-20:blue-theme',
      data: '-blue',
      callback: () => chrome.readingMode.onBlueTheme(),
    },
  ];


  private rateOptions_: number[] = [0.5, 0.8, 1, 1.2, 1.5, 2, 3, 4];

  private moreOptionsButtons_: MenuButton[] = [
    {
      id: 'color',
      icon: 'read-anything:color',
      ariaLabel: loadTimeData.getString('themeTitle'),
      menuToOpen: () => this.$.colorMenu,
    },
    {
      id: 'line-spacing',
      icon: 'read-anything:line-spacing',
      ariaLabel: loadTimeData.getString('lineSpacingTitle'),
      menuToOpen: () => this.$.lineSpacingMenu,
    },
    {
      id: 'letter-spacing',
      icon: 'read-anything:letter-spacing',
      ariaLabel: loadTimeData.getString('letterSpacingTitle'),
      menuToOpen: () => this.$.letterSpacingMenu,
    },
  ];

  private textStyleOptions_: MenuButton[] = [];

  private showAtPositionConfig_: ShowAtPositionConfig = {
    top: 20,
    left: 8,
    anchorAlignmentY: AnchorAlignment.AFTER_END,
  };

  private isReadAloudEnabled_: boolean;
  private isHighlightOn_: boolean = true;
  private activeButton_: HTMLElement|null;

  private toolbarContainerObserver_: ResizeObserver|null;
  private dragResizeCallback_: () => void;

  // If Read Aloud is in the paused state. This is set from the parent element
  // via one way data binding.
  private readonly paused: boolean;

  // If Read Anything has content. If it doesn't, certain toolbar buttons
  // like the play / pause button should be disabled. This is set from
  // the parent element via one way data binding.
  private readonly hasContent: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;
    if (this.isReadAloudEnabled_) {
      this.textStyleOptions_.push(
          {
            id: 'font-size',
            icon: 'read-anything:font-size',
            ariaLabel: loadTimeData.getString('fontSizeTitle'),
            menuToOpen: () => this.$.fontSizeMenu,
          },
          {
            id: 'font',
            icon: 'read-anything:font',
            ariaLabel: loadTimeData.getString('fontNameTitle'),
            menuToOpen: () => this.$.fontMenu,
          },
      );

      const shadowRoot = this.shadowRoot;
      assert(shadowRoot);
      const toolbar = shadowRoot.getElementById('toolbar-container');
      assert(toolbar);

      this.toolbarContainerObserver_ =
          new ResizeObserver(this.onToolbarResize_);
      this.toolbarContainerObserver_.observe(toolbar);

      this.dragResizeCallback_ = this.onDragResize_.bind(this);
      window.addEventListener('resize', this.dragResizeCallback_);
    }
    this.textStyleOptions_ =
        this.textStyleOptions_.concat(this.moreOptionsButtons_);

    this.updateFonts();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.dragResizeCallback_) {
      window.removeEventListener('resize', this.dragResizeCallback_);
    }
    this.toolbarContainerObserver_?.disconnect();
  }

  private onDragResize_() {
    const toolbar =
        this.shadowRoot?.getElementById('toolbar-container') as HTMLElement;
    assert(toolbar);
    ReadAnythingToolbarElement.maybeUpdateMoreOptions(toolbar);
  }

  private onToolbarResize_(entries: ResizeObserverEntry[]) {
    assert(entries.length === 1);
    const toolbar = entries[0].target as HTMLElement;
    ReadAnythingToolbarElement.maybeUpdateMoreOptions(toolbar);
  }

  private restoreFontMenu_() {
    const currentFontIndex =
        this.fontOptions_.indexOf(chrome.readingMode.fontName);
    let fontOptions: Element[];
    if (this.isReadAloudEnabled_) {
      fontOptions = Array.from(this.$.fontMenu.children);
      this.setCheckMarkForMenu_(this.$.fontMenu, currentFontIndex);

      // Setting the custom fonts on each of the elements in the dropdown is
      // technically possible when Read Aloud is disabled, but it can cause
      // an issue where the first instance of opening the dropdown shows a
      // scrollbar because the height is calculated before the font is set.
      // Therefore, only set the custom fonts on the individual items when
      // Read Aloud is enabled.
      this.setFontForFontOptions_(fontOptions);
    } else {
      const shadowRoot = this.shadowRoot;
      assert(shadowRoot);
      const select =
          shadowRoot.getElementById('font-select') as HTMLSelectElement;
      assert(select);
      fontOptions = Array.from(select.options);
      select.selectedIndex = currentFontIndex;
    }
  }

  private setFontForFontOptions_(fontOptions: Element[]) {
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
    this.restoreFontMenu_();

    this.updateLinkToggleButton();

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
    if (this.isReadAloudEnabled_) {
      this.setFontForFontOptions_(Array.from(this.$.fontMenu.children));
    }
  }


  private playPauseButtonAriaLabel_(paused: boolean) {
    return paused ? loadTimeData.getString('playLabel') :
                    loadTimeData.getString('pauseLabel');
  }

  private playPauseButtonIronIcon_(paused: boolean) {
    return paused ? 'read-anything-20:play' : 'read-anything-20:pause';
  }

  private closeMenus_() {
    this.$.rateMenu.close();
    this.$.colorMenu.close();
    this.$.lineSpacingMenu.close();
    this.$.letterSpacingMenu.close();
    this.$.fontMenu.close();
  }

  private emitEvent_(name: string, eventDetail?: any) {
    this.dispatchEvent(new CustomEvent(name, {
      bubbles: true,
      composed: true,
      detail: eventDetail,
    }));
  }

  private onNextGranularityClick_() {
    this.emitEvent_(NEXT_GRANULARITY_EVENT);
  }

  private onPreviousGranularityClick_() {
    this.emitEvent_(PREVIOUS_GRANULARITY_EVENT);
  }

  private onTextStyleMenuButtonClick_(event: DomRepeatEvent<MenuButton>) {
    this.openMenu_(event.model.item.menuToOpen(), event.target as HTMLElement);
  }

  private onShowRateMenuClick_(event: MouseEvent) {
    this.openMenu_(this.$.rateMenu, event.target as HTMLElement);
  }

  private onMoreOptionsClick_(event: MouseEvent) {
    this.openMenu_(this.$.moreOptionsMenu, event.target as HTMLElement);
  }

  private openMenu_(
      menuToOpen: CrActionMenuElement, target: HTMLElement,
      fullScreen: boolean = false) {
    // The button should stay active while the menu is open and deactivate when
    // the menu closes.
    menuToOpen.addEventListener('close', () => {
      target.className = target.className.replace(activeClass, '');
    });
    target.className += activeClass;
    this.closeMenus_();

    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const minY = target.getBoundingClientRect().bottom;
    if (fullScreen) {
      menuToOpen.showAt(target, {
        minY: minY,
        left: 0,
        anchorAlignmentY: AnchorAlignment.AFTER_END,
        noOffset: true,
      });
    } else {
      menuToOpen.showAt(target, {
        minY: minY,
        anchorAlignmentX: AnchorAlignment.AFTER_START,
        anchorAlignmentY: AnchorAlignment.AFTER_END,
        noOffset: true,
      });
    }
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

    this.emitEvent_(HIGHLIGHT_TOGGLE_EVENT, {
      highlightOn: this.isHighlightOn_,
    });
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
    this.propagateFontChange_(fontName);
    this.setCheckMarkForMenu_(this.$.fontMenu, event.model.index);

    this.closeMenus_();
  }

  private onFontSelectValueChange_(event: Event) {
    const fontName = (event.target as HTMLSelectElement).value;
    this.propagateFontChange_(fontName);
  }

  private propagateFontChange_(fontName: string) {
    chrome.readingMode.onFontChange(fontName);
    this.emitEvent_(FONT_EVENT, {
      fontName,
    });
    this.style.fontFamily = validatedFontName(fontName);
  }

  private onRateClick_(event: DomRepeatEvent<number>) {
    chrome.readingMode.onSpeechRateChange(event.model.item);
    this.emitEvent_(RATE_EVENT, {
      rate: event.model.item,
    });
    this.setRateIcon_(event.model.item);
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
      ReadAnythingToolbarElement.hideElement(element, true);
    });
    const checkMark = checkMarks[index] as IronIconElement;
    ReadAnythingToolbarElement.showElement(checkMark);
  }

  private onFontSizeIncreaseClick_() {
    this.updateFontSize_(true);
  }

  private onFontSizeDecreaseClick_() {
    this.updateFontSize_(false);
  }

  private onToggleButtonClick_(event: DomRepeatEvent<ToggleButton>) {
    event.model.item.callback(event);
  }

  private onToggleLinksClick_(event: DomRepeatEvent<ToggleButton>) {
    if (!event.target) {
      return;
    }

    chrome.metricsPrivate.recordEnumerationValue(
        SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.LINKS_ENABLED_CHANGE,
        ReadAnythingSettingsChange.COUNT);

    chrome.readingMode.onLinksEnabledToggled();
    this.emitEvent_(LINKS_EVENT);
    this.updateLinkToggleButton();
  }

  private updateLinkToggleButton() {
    const button = this.shadowRoot?.getElementById(LINK_TOGGLE_BUTTON_ID) as
        CrIconButtonElement;
    if (button) {
      button.ironIcon = chrome.readingMode.linksEnabled ? LINKS_ENABLED_ICON :
                                                          LINKS_DISABLED_ICON;
      button.title = chrome.readingMode.linksEnabled ?
          loadTimeData.getString('disableLinksLabel') :
          loadTimeData.getString('enableLinksLabel');
    }
  }

  private updateFontSize_(increase: boolean) {
    chrome.metricsPrivate.recordEnumerationValue(
        SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.FONT_SIZE_CHANGE,
        ReadAnythingSettingsChange.COUNT);
    chrome.readingMode.onFontSizeChanged(increase);
    this.emitEvent_(FONT_SIZE_EVENT);
    // Don't close the menu
  }

  private onFontResetClick_() {
    chrome.metricsPrivate.recordEnumerationValue(
        SETTINGS_CHANGE_UMA, ReadAnythingSettingsChange.FONT_SIZE_CHANGE,
        ReadAnythingSettingsChange.COUNT);
    chrome.readingMode.onFontSizeReset();
    this.emitEvent_(FONT_SIZE_EVENT);
  }

  private onPlayPauseClick_() {
    this.emitEvent_(PLAY_PAUSE_EVENT);
  }

  private onToolbarKeyDown_(e: KeyboardEvent) {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const toolbar = shadowRoot.getElementById('toolbar-container');
    assert(toolbar);
    const buttons = Array.from(toolbar.querySelectorAll('.toolbar-button')) as
        HTMLElement[];
    assert(buttons);

    // Only allow focus on the currently visible and actionable elements.
    const focusableElements = buttons.filter(el => {
      return (el.clientHeight > 0) && (el.clientWidth > 0) &&
          (el.getBoundingClientRect().right < toolbar.clientWidth) &&
          (el.className !== 'separator');
    });

    // Allow focusing the font selection if it's visible.
    if (!this.isReadAloudEnabled_) {
      const select = shadowRoot.getElementById('font-select') as HTMLElement;
      assert(select);
      focusableElements.unshift(select);
    }

    // Allow focusing the more options menu if it's visible.
    const moreOptionsButton = toolbar.querySelector<HTMLElement>('#more');
    assert(moreOptionsButton);
    if (moreOptionsButton.style.display &&
        (moreOptionsButton.style.display !== 'none')) {
      focusableElements.push(moreOptionsButton);
      Array.from(toolbar.querySelectorAll(moreOptionsClass))
          .forEach(element => {
            focusableElements.push(element as HTMLElement);
          });
    }

    this.onKeyDown_(e, focusableElements);
  }

  private onFontSizeMenuKeyDown_(e: KeyboardEvent) {
    this.onKeyDown_(
        e, Array.from(this.$.fontSizeMenu.children) as HTMLElement[]);
  }

  private onKeyDown_(e: KeyboardEvent, focusableElements: HTMLElement[]) {
    if (!['ArrowRight', 'ArrowLeft'].includes(e.key)) {
      return;
    }

    e.preventDefault();
    const currentIndex = focusableElements.indexOf(e.target as HTMLElement);
    const direction = e.key === 'ArrowRight' ? 1 : -1;
    // Move to the next focusable item in the toolbar, wrapping around
    // if we've reached the end or beginning.
    let newIndex = (currentIndex + direction + focusableElements.length) %
        focusableElements.length;
    // Skip focusing the button itself and go directly to the children. We still
    // need this button in the list of focusable elements because it can become
    // focused by tabbing while the menu is open and we want the arrow key
    // behavior to continue smoothly.
    if (focusableElements[newIndex].id === 'more') {
      newIndex += direction;
    }

    // Open the overflow menu if the next button is in that menu. Close it
    // otherwise.
    const elementToFocus = focusableElements[newIndex];
    assert(elementToFocus);
    if (elementToFocus.className !== moreOptionsClass.slice(1)) {
      this.$.moreOptionsMenu.close();
    } else if (!this.$.moreOptionsMenu.open) {
      const moreOptionsButton =
          focusableElements.find(element => element.id === 'more');
      assert(moreOptionsButton);
      this.openMenu_(this.$.moreOptionsMenu, moreOptionsButton);
    }

    // When the user tabs away from the toolbar and then tabs back, we want to
    // focus the last focused item in the toolbar
    focusableElements.forEach(el => {
      el.tabIndex = -1;
    });
    elementToFocus.tabIndex = 0;

    // Wait for the next animation frame for the overflow menu to show or hide.
    requestAnimationFrame(() => {
      elementToFocus.focus();
    });
  }

  private onFontSelectKeyDown_(e: KeyboardEvent) {
    // The default behavior goes to the next select option. However, we want
    // to instead go to the next toolbar button (handled in onToolbarKeyDown_).
    // ArrowDown and ArrowUp will still move to the next/previous option.
    if (['ArrowRight', 'ArrowLeft'].includes(e.key)) {
      e.preventDefault();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-toolbar': ReadAnythingToolbarElement;
  }
}

customElements.define('read-anything-toolbar', ReadAnythingToolbarElement);
