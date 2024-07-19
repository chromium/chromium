// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './voice_selection_menu.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/md_select.css.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconElement} from '//resources/cr_elements/cr_icon/cr_icon.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Debouncer, PolymerElement, timeOut} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getCurrentSpeechRate, minOverflowLengthToScroll, openMenu, spinnerDebounceTimeout, ToolbarEvent} from './common.js';
import {ReadAloudSettingsChange, ReadAnythingSettingsChange} from './metrics_browser_proxy.js';
import {ReadAnythingLogger, TimeFrom, TimeTo} from './read_anything_logger.js';
import {getTemplate} from './read_anything_toolbar.html.js';
import type {VoiceSelectionMenuElement} from './voice_selection_menu.js';

export interface ReadAnythingToolbarElement {
  $: {
    rateMenu: CrLazyRenderElement<CrActionMenuElement>,
    colorMenu: CrLazyRenderElement<CrActionMenuElement>,
    lineSpacingMenu: CrLazyRenderElement<CrActionMenuElement>,
    letterSpacingMenu: CrLazyRenderElement<CrActionMenuElement>,
    fontMenu: CrLazyRenderElement<CrActionMenuElement>,
    fontSizeMenu: CrLazyRenderElement<CrActionMenuElement>,
    moreOptionsMenu: CrLazyRenderElement<CrActionMenuElement>,
    voiceSelectionMenu: VoiceSelectionMenuElement,
    toolbarContainer: HTMLElement,
    more: CrIconButtonElement,
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

export const moreOptionsClass = '.more-options-icon';

// Link toggle button constants.
export const LINKS_ENABLED_ICON = 'read-anything:links-enabled';
export const LINKS_DISABLED_ICON = 'read-anything:links-disabled';
export const LINK_TOGGLE_BUTTON_ID = 'link-toggle-button';

// Images toggle button constants.
export const IMAGES_ENABLED_ICON = 'read-anything:images-enabled';
export const IMAGES_DISABLED_ICON = 'read-anything:images-disabled';
export const IMAGES_TOGGLE_BUTTON_ID = 'images-toggle-button';

// Constants for styling the toolbar when page zoom changes.
const whiteSpaceTypical = 'nowrap';
const whiteSpaceOverflow = 'normal';

const ReadAnythingToolbarElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));
export class ReadAnythingToolbarElement extends ReadAnythingToolbarElementBase {
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
      rateOptions: Array,
      textStyleOptions_: Array,
      textStyleToggles_: Array,
      isSpeechActive: Boolean,
      isAudioCurrentlyPlaying: Boolean,
      isReadAloudPlayable: Boolean,
      selectedVoice: Object,
      voicePackInstallStatus: Map,
      availableVoices: Array,
      enabledLangs: Array,
      localeToDisplayName: Object,
      previewVoicePlaying: Object,
      areFontsLoaded_: Boolean,
      lastDownloadedLang: String,
    };
  }

  static get observers() {
    return [
      'onSpeechPlayingStateChanged_(isSpeechActive, isAudioCurrentlyPlaying)',
    ];
  }

  // This function has to be static because it's called from the ResizeObserver
  // callback which doesn't have access to "this"
  static maybeUpdateMoreOptions(toolbar: HTMLElement) {
    // Hide the more options button first to calculate if we need it
    const moreOptionsButton = toolbar.querySelector<HTMLElement>('#more');
    assert(moreOptionsButton, 'more options button doesn\'t exist');
    ReadAnythingToolbarElement.hideElement(moreOptionsButton, false);

    // Show all the buttons to see if they fit.
    const buttons =
        Array.from(toolbar.querySelectorAll<HTMLElement>('.text-style-button'));
    assert(buttons, 'no toolbar buttons');
    buttons.forEach(btn => ReadAnythingToolbarElement.showElement(btn));
    toolbar.dispatchEvent(new CustomEvent('reset-toolbar', {
      bubbles: true,
      composed: true,
    }));

    if (!toolbar.offsetParent) {
      return;
    }

    // When the toolbar's width exceeds the parent width, then the content has
    // overflowed.
    const parentWidth = toolbar.offsetParent.clientWidth;
    if (toolbar.clientWidth > parentWidth) {
      // Hide at least 3 buttons and more if needed.
      let numOverflowButtons = 3;
      let nextOverflowButton = buttons[buttons.length - numOverflowButtons];
      // No need to hide a button if it only exceeds the width by a little (i.e.
      // only the padding overflows).
      const maxDiff = 10;
      let overflowLength = nextOverflowButton.offsetLeft +
          nextOverflowButton.offsetWidth - parentWidth;
      while (overflowLength > maxDiff) {
        numOverflowButtons++;
        nextOverflowButton = buttons[buttons.length - numOverflowButtons];
        if (!nextOverflowButton) {
          break;
        }

        overflowLength = nextOverflowButton.offsetLeft +
            nextOverflowButton.offsetWidth - parentWidth;
      }

      // Notify the app and toolbar of the overflow.
      toolbar.dispatchEvent(new CustomEvent('toolbar-overflow', {
        bubbles: true,
        composed: true,
        detail: {numOverflowButtons, overflowLength},
      }));

      // If we have too much overflow, we won't use the more options button.
      if (numOverflowButtons > buttons.length) {
        return;
      }

      // Hide the overflowed buttons and show the more options button in front
      // of them.
      ReadAnythingToolbarElement.showElement(moreOptionsButton);
      const overflowedButtons =
          buttons.slice(buttons.length - numOverflowButtons);
      overflowedButtons.forEach(
          btn => ReadAnythingToolbarElement.hideElement(btn, true));
      toolbar.insertBefore(moreOptionsButton, overflowedButtons[0]);
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

  private startTime = Date.now();
  private constructorTime: number;

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


  rateOptions: number[] = [0.5, 0.8, 1, 1.2, 1.5, 2, 3, 4];

  private moreOptionsButtons_: MenuButton[] = [];

  private textStyleOptions_: MenuButton[] = [
    {
      id: 'color',
      icon: 'read-anything:color',
      ariaLabel: loadTimeData.getString('themeTitle'),
      menuToOpen: () => this.$.colorMenu.get(),
    },
    {
      id: 'line-spacing',
      icon: 'read-anything:line-spacing',
      ariaLabel: loadTimeData.getString('lineSpacingTitle'),
      menuToOpen: () => this.$.lineSpacingMenu.get(),
    },
    {
      id: 'letter-spacing',
      icon: 'read-anything:letter-spacing',
      ariaLabel: loadTimeData.getString('letterSpacingTitle'),
      menuToOpen: () => this.$.letterSpacingMenu.get(),
    },
  ];

  private isReadAloudEnabled_: boolean;
  private activeButton_: HTMLElement|null;
  private areFontsLoaded_: boolean = false;
  private colorSuffix_: string = '';

  private currentFocusId_: string = '';
  private toolbarContainerObserver_: ResizeObserver|null;
  private windowResizeCallback_: () => void;

  // If Read Aloud is playing speech. This is set from the parent element via
  // one way data binding.
  isSpeechActive: boolean;

  // If speech is actually playing. Due to latency with the TTS engine, there
  // can be a delay between when the user presses play and speech actually
  // plays.
  private isAudioCurrentlyPlaying: boolean;

  private hideSpinner: boolean = true;

  private debouncer_: Debouncer|null = null;

  // If Read Aloud is playable. Certain states, such as when Read Anything does
  // not have content or when the speech engine is loading should disable
  // certain toolbar buttons like the play / pause button should be disabled.
  // This is set from the parent element via one way data binding.
  private readonly isReadAloudPlayable: boolean;

  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  constructor() {
    super();
    this.constructorTime = Date.now();
    this.logger_.logTimeBetween(
        TimeFrom.TOOLBAR, TimeTo.CONSTRUCTOR, this.startTime,
        this.constructorTime);
    this.isReadAloudEnabled_ = chrome.readingMode.isReadAloudEnabled;

    // Only add the button to the toolbar if the feature is enabled.
    if (chrome.readingMode.imagesFeatureEnabled) {
      this.textStyleToggles_.push({
        id: IMAGES_TOGGLE_BUTTON_ID,
        icon: chrome.readingMode.imagesEnabled ? IMAGES_ENABLED_ICON :
                                                 IMAGES_DISABLED_ICON,
        title: chrome.readingMode.imagesEnabled ?
            loadTimeData.getString('disableImagesLabel') :
            loadTimeData.getString('enableImagesLabel'),
        callback: this.onToggleImagesClick_.bind(this),
      });
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    const connectedCallbackTime = Date.now();
    this.logger_.logTimeBetween(
        TimeFrom.TOOLBAR, TimeTo.CONNNECTED_CALLBACK, this.startTime,
        connectedCallbackTime);
    this.logger_.logTimeBetween(
        TimeFrom.TOOLBAR_CONSTRUCTOR, TimeTo.CONNNECTED_CALLBACK,
        this.constructorTime, connectedCallbackTime);
    if (this.isReadAloudEnabled_) {
      this.textStyleOptions_.unshift(
          {
            id: 'font-size',
            icon: 'read-anything:font-size',
            ariaLabel: loadTimeData.getString('fontSizeTitle'),
            menuToOpen: () => this.$.fontSizeMenu.get(),
          },
          {
            id: 'font',
            icon: 'read-anything:font',
            ariaLabel: loadTimeData.getString('fontNameTitle'),
            menuToOpen: () => this.$.fontMenu.get(),
          },
      );
    }
    this.toolbarContainerObserver_ = new ResizeObserver(this.onToolbarResize_);
    this.toolbarContainerObserver_.observe(this.$.toolbarContainer);
    this.windowResizeCallback_ = this.onWindowResize_.bind(this);
    window.addEventListener('resize', this.windowResizeCallback_);

    this.initFonts_();
    this.loadFontsStylesheet();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.windowResizeCallback_) {
      window.removeEventListener('resize', this.windowResizeCallback_);
    }
    this.toolbarContainerObserver_?.disconnect();
  }

  // Loading the fonts stylesheet can take a while, especially with slow
  // Internet connections. Since we don't want this to block the rest of
  // Reading Mode from loading, we load this stylesheet asynchronously
  // in TypeScript instead of in read_anything.html
  async loadFontsStylesheet() {
    const link = document.createElement('link');
    link.rel = 'preload';
    link.as = 'style';
    link.href = 'https://fonts.googleapis.com/css?family=';
    link.href += chrome.readingMode.allFonts.join('|');
    link.href = link.href.replace(' ', '+');

    link.addEventListener('load', () => {
      link.media = 'all';
      link.rel = 'stylesheet';
      this.setFontsLoaded();
    });

    document.head.appendChild(link);
  }

  setFontsLoaded() {
    this.areFontsLoaded_ = true;
  }

  private onResetToolbar_() {
    this.$.moreOptionsMenu.getIfExists()?.close();
    this.moreOptionsButtons_ = [];
    this.updateStyles({
      '--toolbar-white-space': whiteSpaceTypical,
    });
  }

  private onToolbarOverflow_(
      event:
          CustomEvent<{numOverflowButtons: number, overflowLength: number}>) {
    const firstHiddenButton =
        this.textStyleOptions_.length - event.detail.numOverflowButtons;
    // Wrap the buttons if we overflow significantly but aren't yet scrolling
    // the whole app.
    if (firstHiddenButton < 0 &&
        event.detail.overflowLength < minOverflowLengthToScroll) {
      this.updateStyles({
        '--toolbar-white-space': whiteSpaceOverflow,
      });
      return;
    }

    // If we only overflow by a little, use the more options button.
    this.moreOptionsButtons_ = this.textStyleOptions_.slice(firstHiddenButton);
  }

  private onWindowResize_() {
    ReadAnythingToolbarElement.maybeUpdateMoreOptions(this.$.toolbarContainer);
  }

  private onToolbarResize_(entries: ResizeObserverEntry[]) {
    assert(entries.length === 1, 'resize observer is expecting one entry');
    const toolbar = entries[0].target as HTMLElement;
    ReadAnythingToolbarElement.maybeUpdateMoreOptions(toolbar);
  }

  private restoreFontMenu_() {
    // Default to the first font option if the previously used font is no
    // longer available.
    let currentFontIndex =
        this.fontOptions_.indexOf(chrome.readingMode.fontName);
    if (currentFontIndex < 0) {
      currentFontIndex = 0;
      this.propagateFontChange_(this.fontOptions_[0]);
    }
    if (this.isReadAloudEnabled_) {
      this.setCheckMarkForMenu_(
          this.$.fontMenu.getIfExists(), currentFontIndex);

    } else {
      const select = this.$.toolbarContainer.querySelector<HTMLSelectElement>(
          '#font-select');
      assert(select, 'no font select menu');
      select.selectedIndex = currentFontIndex;
    }
  }

  restoreSettingsFromPrefs(colorSuffix?: string) {
    this.colorSuffix_ = colorSuffix ? colorSuffix : '';
    this.restoreFontMenu_();

    this.updateLinkToggleButton();

    if (this.isReadAloudEnabled_) {
      const speechRate = getCurrentSpeechRate();
      this.setRateIcon_(speechRate);
      this.setCheckMarkForMenu_(
          this.$.rateMenu.getIfExists(), this.rateOptions.indexOf(speechRate));

      this.setHighlightState_(chrome.readingMode.isHighlightOn());
    }
    this.setCheckMarkForMenu_(
        this.$.colorMenu.getIfExists(),
        this.getIndexOfSetting_(this.colorOptions_, colorSuffix));
    this.setCheckMarkForMenu_(
        this.$.lineSpacingMenu.getIfExists(),
        this.getIndexOfSetting_(
            this.lineSpacingOptions_, this.getCurrentLineSpacing()));
    this.setCheckMarkForMenu_(
        this.$.letterSpacingMenu.getIfExists(),
        this.getIndexOfSetting_(
            this.letterSpacingOptions_, this.getCurrentLetterSpacing()));
  }

  private getIndexOfSetting_(
      menuArray: Array<MenuStateItem<any>>, dataToFind: any): number {
    return menuArray.findIndex((item) => (item.data === dataToFind));
  }

  updateFonts() {
    this.initFonts_();
    this.restoreFontMenu_();
  }

  private initFonts_() {
    this.fontOptions_ = Object.assign([], chrome.readingMode.supportedFonts);
  }

  private isFontItemSelected_(item: number): boolean {
    return item !== this.fontOptions_.indexOf(chrome.readingMode.fontName);
  }

  private isColorItemSelected_(item: number): boolean {
    return item !==
        this.getIndexOfSetting_(this.colorOptions_, this.colorSuffix_);
  }

  private isLineSpacingItemSelected_(item: number): boolean {
    return item !==
        this.getIndexOfSetting_(
            this.lineSpacingOptions_, this.getCurrentLineSpacing());
  }

  private isLetterSpacingItemSelected_(item: number): boolean {
    return item !==
        this.getIndexOfSetting_(
            this.letterSpacingOptions_, this.getCurrentLetterSpacing());
  }

  private isRateItemSelected_(item: number): boolean {
    return item !== this.rateOptions.indexOf(getCurrentSpeechRate());
  }

  private getCurrentLineSpacing(): number {
    return parseFloat(chrome.readingMode.lineSpacing.toFixed(2));
  }

  private getCurrentLetterSpacing(): number {
    return parseFloat(chrome.readingMode.letterSpacing.toFixed(2));
  }

  // Instead of using areFontsLoaded_ directly in this method, we pass
  // the variable through HTML to force a re-render when the variable changes.
  private getFontItemLabel_(item: string, areFontsLoaded: boolean): string {
    // Before fonts are loaded, append the loading text to the font names
    // so that the names will appear in the font menu like:
    // Poppins (loading).
    return areFontsLoaded ?
        `${item}` :
        `${item}\u00A0${this.i18n('readingModeFontLoadingText')}`;
  }

  // TODO(b/339007175): Consider using something like "Playback toggle" as
  // an aria label instead of "Play" to be more accurate.
  private playPauseButtonAriaLabel_() {
    return loadTimeData.getString('playLabel');
  }

  private playPauseButtonTitle_() {
    return loadTimeData.getString(
        this.isSpeechActive ? 'pauseTooltip' : 'playTooltip');
  }

  private playPauseButtonIronIcon_() {
    return this.isSpeechActive ? 'read-anything-20:pause' :
                                 'read-anything-20:play';
  }

  private closeMenus_() {
    this.$.rateMenu.getIfExists()?.close();
    this.$.colorMenu.getIfExists()?.close();
    this.$.lineSpacingMenu.getIfExists()?.close();
    this.$.letterSpacingMenu.getIfExists()?.close();
    this.$.fontMenu.getIfExists()?.close();
  }

  private emitEvent_(name: string, eventDetail?: any) {
    this.dispatchEvent(new CustomEvent(name, {
      bubbles: true,
      composed: true,
      detail: eventDetail,
    }));
  }

  private onNextGranularityClick_() {
    this.emitEvent_(ToolbarEvent.NEXT_GRANULARITY);
  }

  private onPreviousGranularityClick_() {
    this.emitEvent_(ToolbarEvent.PREVIOUS_GRANULARITY);
  }

  private onTextStyleMenuButtonClick_(event: DomRepeatEvent<MenuButton>) {
    openMenu(event.model.item.menuToOpen(), event.target as HTMLElement);
  }

  private onShowRateMenuClick_(event: MouseEvent) {
    openMenu(this.$.rateMenu.get(), event.target as HTMLElement);
  }

  private onVoiceSelectionMenuClick_(event: MouseEvent) {
    const voiceMenu =
        this.$.toolbarContainer.querySelector('#voiceSelectionMenu');
    assert(voiceMenu, 'no voiceMenu element');
    (voiceMenu as VoiceSelectionMenuElement).onVoiceSelectionMenuClick(event);
  }

  private onMoreOptionsClick_(event: MouseEvent) {
    const menu = this.$.moreOptionsMenu.get();
    openMenu(menu, event.target as HTMLElement);
  }

  private onHighlightClick_() {
    this.logger_.logSpeechSettingsChange(
        ReadAloudSettingsChange.HIGHLIGHT_CHANGE);
    const isHighlightOn = chrome.readingMode.isHighlightOn();
    if (isHighlightOn) {
      chrome.readingMode.turnedHighlightOff();
    } else {
      chrome.readingMode.turnedHighlightOn();
    }

    this.logger_.logHighlightState(!isHighlightOn);
    this.setHighlightState_(!isHighlightOn);
  }

  private setHighlightState_(turnOn: boolean) {
    const button = this.$.toolbarContainer.querySelector('#highlight');
    assert(button, 'no highlight button');
    if (turnOn) {
      button.setAttribute('iron-icon', 'read-anything:highlight-on');
      button.setAttribute('title', loadTimeData.getString('turnHighlightOff'));
    } else {
      button.setAttribute('iron-icon', 'read-anything:highlight-off');
      button.setAttribute('title', loadTimeData.getString('turnHighlightOn'));
    }

    this.emitEvent_(ToolbarEvent.HIGHLIGHT_TOGGLE, {
      highlightOn: turnOn,
    });
  }

  private onLetterSpacingClick_(event: DomRepeatEvent<MenuStateItem<number>>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
        this.$.letterSpacingMenu.get(), ToolbarEvent.LETTER_SPACING);
  }

  private onLineSpacingClick_(event: DomRepeatEvent<MenuStateItem<number>>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE,
        this.$.lineSpacingMenu.get(), ToolbarEvent.LINE_SPACING);
  }

  private onColorClick_(event: DomRepeatEvent<MenuStateItem<string>>) {
    this.onTextStyleClick_(
        event, ReadAnythingSettingsChange.THEME_CHANGE, this.$.colorMenu.get(),
        ToolbarEvent.THEME);
  }

  private onTextStyleClick_(
      event: DomRepeatEvent<MenuStateItem<any>>,
      logVal: ReadAnythingSettingsChange, menuClicked: CrActionMenuElement,
      emitEventName: string) {
    event.model.item.callback();
    this.logger_.logTextSettingsChange(logVal);
    this.emitEvent_(emitEventName, {data: event.model.item.data});
    this.setCheckMarkForMenu_(menuClicked, event.model.index);
    this.closeMenus_();
  }

  private onFontClick_(event: DomRepeatEvent<string>) {
    this.logger_.logTextSettingsChange(ReadAnythingSettingsChange.FONT_CHANGE);
    const fontName = event.model.item;
    this.propagateFontChange_(fontName);
    this.setCheckMarkForMenu_(this.$.fontMenu.getIfExists(), event.model.index);

    this.closeMenus_();
  }

  private onFontSelectValueChange_(event: Event) {
    const fontName = (event.target as HTMLSelectElement).value;
    this.propagateFontChange_(fontName);
  }

  private propagateFontChange_(fontName: string) {
    chrome.readingMode.onFontChange(fontName);
    this.emitEvent_(ToolbarEvent.FONT, {
      fontName,
    });
    this.style.fontFamily = chrome.readingMode.getValidatedFontName(fontName);
  }

  private onRateClick_(event: DomRepeatEvent<number>) {
    this.logger_.logSpeechSettingsChange(
        ReadAloudSettingsChange.VOICE_SPEED_CHANGE);
    // Log which rate is chosen by index rather than the rate value itself.
    this.logger_.logVoiceSpeed(event.model.index);
    chrome.readingMode.onSpeechRateChange(event.model.item);
    this.emitEvent_(ToolbarEvent.RATE);
    this.setRateIcon_(event.model.item);
    this.setCheckMarkForMenu_(this.$.rateMenu.getIfExists(), event.model.index);

    this.closeMenus_();
  }

  private setRateIcon_(rate: number) {
    const button = this.$.toolbarContainer.querySelector('#rate');
    assert(button, 'no rate button');
    button?.setAttribute('iron-icon', 'voice-rate:' + rate);
    button?.setAttribute('aria-label', this.getVoiceSpeedLabel_(rate));
  }

  private setCheckMarkForMenu_(menu: CrActionMenuElement|null, index: number) {
    // If the menu has not yet been rendered, don't attempt to set any check
    // marks yet.
    if (!menu) {
      return;
    }
    const checked =
        Array.from(menu.getElementsByClassName('check-mark-hidden-false'));
    checked.forEach(element => {
      const iconElement = element as CrIconElement;
      if (iconElement) {
        iconElement.classList.toggle('check-mark-hidden-true', true);
        iconElement.classList.toggle('check-mark-hidden-false', false);
      }
    });

    const checkMarks = Array.from(menu.getElementsByClassName('check-mark'));
    const checkMark = checkMarks[index] as CrIconElement;
    if (checkMark) {
      checkMark.classList.toggle('check-mark-hidden-true', false);
      checkMark.classList.toggle('check-mark-hidden-false', true);
    }
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

    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINKS_ENABLED_CHANGE);

    chrome.readingMode.onLinksEnabledToggled();
    this.emitEvent_(ToolbarEvent.LINKS);
    this.updateLinkToggleButton();
  }

  private onToggleImagesClick_(event: DomRepeatEvent<ToggleButton>) {
    if (!event.target) {
      return;
    }
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.IMAGES_ENABLED_CHANGE);

    chrome.readingMode.onImagesEnabledToggled();
    this.emitEvent_(ToolbarEvent.IMAGES);
    this.updateImagesToggleButton();
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

  private updateImagesToggleButton() {
    const button = this.shadowRoot?.getElementById(IMAGES_TOGGLE_BUTTON_ID) as
        CrIconButtonElement;
    if (button) {
      button.ironIcon = chrome.readingMode.imagesEnabled ? IMAGES_ENABLED_ICON :
                                                           IMAGES_DISABLED_ICON;
      button.title = chrome.readingMode.imagesEnabled ?
          loadTimeData.getString('disableImagesLabel') :
          loadTimeData.getString('enableImagesLabel');
    }
  }

  private updateFontSize_(increase: boolean) {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.FONT_SIZE_CHANGE);
    chrome.readingMode.onFontSizeChanged(increase);
    this.emitEvent_(ToolbarEvent.FONT_SIZE);
    // Don't close the menu
  }

  private onFontResetClick_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.FONT_SIZE_CHANGE);
    chrome.readingMode.onFontSizeReset();
    this.emitEvent_(ToolbarEvent.FONT_SIZE);
  }

  private onPlayPauseClick_() {
    this.emitEvent_(ToolbarEvent.PLAY_PAUSE);
  }

  private onToolbarKeyDown_(e: KeyboardEvent) {
    const toolbar = this.$.toolbarContainer;
    const buttons = Array.from(toolbar.querySelectorAll('.toolbar-button')) as
        HTMLElement[];
    assert(buttons, 'no toolbar buttons');

    // Only allow focus on the currently visible and actionable elements.
    const focusableElements = buttons.filter(el => {
      return (el.clientHeight > 0) && (el.clientWidth > 0) &&
          (el.getBoundingClientRect().right < toolbar.clientWidth) &&
          (el.style.visibility !== 'hidden') && (el.style.display !== 'none') &&
          (!(el as any).disabled) && (el.className !== 'separator');
    });

    // Allow focusing the font selection if it's visible.
    if (!this.isReadAloudEnabled_) {
      const select = toolbar.querySelector<HTMLSelectElement>('#font-select');
      assert(select, 'no font select menu');
      focusableElements.unshift(select);
    }

    // Allow focusing the more options menu if it's visible.
    const moreOptionsButton = this.$.more;
    assert(moreOptionsButton, 'no more options button');
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

  private getNewIndex_(e: KeyboardEvent, focusableElements: HTMLElement[]):
      number {
    let currentIndex = focusableElements.indexOf(e.target as HTMLElement);
    const direction =
        (e.key === 'ArrowRight' || e.key === 'ArrowDown') ? 1 : -1;
    // If e.target wasn't found in focusable elements, and we're going
    // backwards, adjust currentIndex so we move to the last focusable element
    if (currentIndex === -1 && direction === -1) {
      currentIndex = focusableElements.length;
    }
    // Move to the next focusable item in the menu, wrapping around
    // if we've reached the end or beginning.
    return (currentIndex + direction + focusableElements.length) %
        focusableElements.length;
  }

  private onFontSizeMenuKeyDown_(e: KeyboardEvent) {
    // The font size selection menu is laid out horizontally, so users should be
    // able to navigate it using either up and down arrows, or left and right
    // arrows.
    if (!['ArrowRight', 'ArrowLeft', 'ArrowUp', 'ArrowDown'].includes(e.key)) {
      return;
    }
    e.preventDefault();
    const focusableElements =
        Array.from(this.$.fontSizeMenu.get().children) as HTMLElement[];
    const elementToFocus =
        focusableElements[this.getNewIndex_(e, focusableElements)];
    assert(elementToFocus, 'no element to focus');
    elementToFocus.focus();
  }

  private getMoreOptionsButtons_(): HTMLElement[] {
    return Array.from(
        this.$.toolbarContainer.querySelectorAll(moreOptionsClass));
  }

  private onKeyDown_(e: KeyboardEvent, focusableElements: HTMLElement[]) {
    if (!['ArrowRight', 'ArrowLeft'].includes(e.key)) {
      return;
    }

    e.preventDefault();
    //  Move to the next focusable item in the toolbar, wrapping around
    //  if we've reached the end or beginning.
    let newIndex = this.getNewIndex_(e, focusableElements);
    const direction = e.key === 'ArrowRight' ? 1 : -1;
    // If the next item has overflowed, skip focusing the more options button
    // itself and go directly to the children. We still need this button in the
    // list of focusable elements because it can become focused by tabbing while
    // the menu is open and we want the arrow key behavior to continue smoothly.
    const elementToFocus = focusableElements[newIndex];
    if (elementToFocus.id === 'more' ||
        elementToFocus.classList.contains(moreOptionsClass.slice(1))) {
      const moreOptionsRendered = this.$.moreOptionsMenu.getIfExists();
      // If the more options menu has not been rendered yet, render it and wait
      // for it to be drawn so we can get the number of elements in the menu.
      if (!moreOptionsRendered || !moreOptionsRendered.open) {
        openMenu(this.$.moreOptionsMenu.get(), this.$.more);
        requestAnimationFrame(() => {
          const moreOptions = this.getMoreOptionsButtons_();
          focusableElements = focusableElements.concat(moreOptions);
          newIndex = (direction === 1) ? (newIndex + 1) :
                                         (focusableElements.length - 1);
          this.updateFocus_(focusableElements, newIndex);
        });
        return;
      }
    }
    this.updateFocus_(focusableElements, newIndex);
  }


  private onSpeechPlayingStateChanged_() {
    // Use a debouncer to reduce glitches. Even when audio is fast to respond to
    // the play button, there are still milliseconds of delay. To prevent the
    // spinner from quickly appearing and disappearing, we use a debouncer. If
    // either the values of `isSpeechActive` or `isAudioCurrentlyPlaying`
    // change, the previously scheduled callback is canceled and a new callback
    // is scheduled.
    // TODO (b/339860819) improve debouncer logic so that the spinner disappears
    // immediately when speech starts playing, or when the pause button is hit.
    this.debouncer_ = Debouncer.debounce(
        this.debouncer_, timeOut.after(spinnerDebounceTimeout), () => {
          this.hideSpinner =
              !this.isSpeechActive || this.isAudioCurrentlyPlaying;
        });
    // If the previously focused item becomes disabled or disappears from the
    // toolbar because of speech starting or stopping, put the focus on the
    // play/pause button so keyboard navigation continues working.
    if ((this.shadowRoot !== null) &&
        (this.shadowRoot.activeElement === null ||
         this.shadowRoot.activeElement.clientHeight === 0)) {
      this.$.toolbarContainer.querySelector<HTMLElement>('#play-pause')
          ?.focus();
    }
  }


  private updateFocus_(focusableElements: HTMLElement[], newIndex: number) {
    const elementToFocus = focusableElements[newIndex];
    assert(elementToFocus, 'no element to focus');

    // When the user tabs away from the toolbar and then tabs back, we want to
    // focus the last focused item in the toolbar
    focusableElements.forEach(el => {
      el.tabIndex = -1;
    });
    this.currentFocusId_ = elementToFocus.id;

    // If a more options button is focused and we tab away, we need to tab
    // back to the more options button instead of the item inside the menu since
    // the menu closes when we tab away.
    if (elementToFocus.classList.contains(moreOptionsClass.slice(1))) {
      this.$.more.tabIndex = 0;
    } else {
      elementToFocus.tabIndex = 0;
      // Close the overflow menu if the next button is not in the menu.
      this.$.moreOptionsMenu.getIfExists()?.close();
    }

    // Wait for the next animation frame for the overflow menu to show or hide.
    requestAnimationFrame(() => {
      elementToFocus.focus();
    });
  }

  private getRateTabIndex_(isReadAloudPlayable: boolean): number {
    return (!isReadAloudPlayable || this.currentFocusId_ === 'rate') ? 0 : -1;
  }

  private onFontSelectKeyDown_(e: KeyboardEvent) {
    // The default behavior goes to the next select option. However, we want
    // to instead go to the next toolbar button (handled in onToolbarKeyDown_).
    // ArrowDown and ArrowUp will still move to the next/previous option.
    if (['ArrowRight', 'ArrowLeft'].includes(e.key)) {
      e.preventDefault();
    }
  }

  // When Read Aloud is enabled, we want the aria label of the toolbar
  // convey information about Read Aloud.
  private getToolbarAriaLabel_(): string {
    return this.isReadAloudEnabled_ ?
        this.i18n('readingModeReadAloudToolbarLabel') :
        this.i18n('readingModeToolbarLabel');
  }

  private getVoiceSpeedLabel_(rate: number = getCurrentSpeechRate()): string {
    return loadTimeData.getStringF('voiceSpeedWithRateLabel', rate);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-toolbar': ReadAnythingToolbarElement;
  }
}

customElements.define('read-anything-toolbar', ReadAnythingToolbarElement);
