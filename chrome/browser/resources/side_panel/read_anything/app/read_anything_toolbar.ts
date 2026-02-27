// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '../read_aloud/voice_selection_menu.js';
import '../menus/simple_action_menu.js';
import '../menus/color_menu.js';
import '../menus/font_menu.js';
import '../menus/line_focus_menu.js';
import '../menus/line_spacing_menu.js';
import '../menus/letter_spacing_menu.js';
import '../menus/highlight_menu.js';
import '../menus/rate_menu.js';
import '../menus/presentation_menu.js';
import '../menus/settings_menu.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/icons.html.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement, html, type TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {DEFAULT_SETTINGS, SettingsOption, ToolbarEvent} from '../content/read_anything_types.js';
import type {LineFocusMovement, LineFocusStyle, SettingsPrefs} from '../content/read_anything_types.js';
import type {ColorMenuElement} from '../menus/color_menu.js';
import type {FontMenuElement} from '../menus/font_menu.js';
import type {HighlightMenuElement} from '../menus/highlight_menu.js';
import type {LetterSpacingMenuElement} from '../menus/letter_spacing_menu.js';
import type {LineFocusMenuElement} from '../menus/line_focus_menu.js';
import type {LineSpacingMenuElement} from '../menus/line_spacing_menu.js';
import type {ToolbarMenu} from '../menus/menu_util.js';
import type {PresentationMenuElement} from '../menus/presentation_menu.js';
import type {RateMenuElement} from '../menus/rate_menu.js';
import type {SettingsMenuElement} from '../menus/settings_menu.js';
import {getCurrentSpeechRate} from '../read_aloud/speech_presentation_rules.js';
import type {VoiceSelectionMenuElement} from '../read_aloud/voice_selection_menu.js';
import {minOverflowLengthToScroll, openMenu, spinnerDebounceTimeout} from '../shared/common.js';
import {getNewIndex, isArrow, isForwardArrow, isHorizontalArrow} from '../shared/keyboard_util.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger, SpeechControls, TimeFrom} from '../shared/read_anything_logger.js';

import {getCss} from './read_anything_toolbar.css.js';
import {getHtml} from './read_anything_toolbar.html.js';

export interface ReadAnythingToolbarElement {
  $: {
    rateMenu: RateMenuElement,
    colorMenu: ColorMenuElement,
    lineSpacingMenu: LineSpacingMenuElement,
    letterSpacingMenu: LetterSpacingMenuElement,
    fontMenu: FontMenuElement,
    fontSizeMenu: CrLazyRenderLitElement<CrActionMenuElement>,
    moreOptionsMenu: CrLazyRenderLitElement<CrActionMenuElement>,
    voiceSelectionMenu: VoiceSelectionMenuElement,
    highlightMenu: HighlightMenuElement,
    lineFocusMenu: LineFocusMenuElement,
    toolbarContainer: HTMLElement,
    more: CrIconButtonElement,
    settingsMenu: SettingsMenuElement,
    presentationMenu: PresentationMenuElement,
  };
}
interface MenuButton {
  id: string;
  icon: string;
  ariaLabel: string;
  openMenu: (target: HTMLElement) => void;
  announceBlock?: TemplateResult;
}


type ToggleButtonId =
    typeof LINK_TOGGLE_BUTTON_ID|typeof IMAGES_TOGGLE_BUTTON_ID;
interface ToggleButton {
  id: ToggleButtonId;
  icon: string;
  title: string;
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

// Max number of paragraph elements inside an aria-live region for
// announcing setting changes. Not clearing the element may make
// the announce block too big and waste memory. Trade-off is that every
// MAX_PARAGRAOHS_IN_ANNOUNCE_BLOCK font sizes, there is a chance the
// announcement won't happen the sixth time, if the change is too fast.
// It is unlikely someone will change the font size more than 5 times so
// this covers most use cases.
const MAX_PARAGRAPHS_IN_ANNOUNCE_BLOCK = 5;

// Constants for styling the toolbar when page zoom changes.
const flexWrapTypical = 'nowrap';
const flexWrapOverflow = 'wrap';

const ReadAnythingToolbarElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class ReadAnythingToolbarElement extends ReadAnythingToolbarElementBase {
  static get is() {
    return 'read-anything-toolbar';
  }
  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isSpeechActive: {type: Boolean},
      isAudioCurrentlyPlaying: {type: Boolean},
      isReadAloudPlayable: {type: Boolean},
      selectedVoice: {type: Object},
      availableVoices: {type: Array},
      enabledLangs: {type: Array},
      localeToDisplayName: {type: Object},
      previewVoicePlaying: {type: Object},
      settingsPrefs: {type: Object},
      areFontsLoaded_: {type: Boolean},
      textStyleOptions_: {type: Array},
      textStyleToggles_: {type: Array},
      hideSpinner_: {type: Boolean},
      speechRate_: {type: Number},
      moreOptionsButtons_: {type: Array},
      pageLanguage: {type: String},
      presentationState: {type: Number},
      isImmersiveMode: {type: Boolean},
      isReadAnythingPinned: {type: Boolean},
      isImmersiveEnabled_: {type: Boolean},
      lineFocusStyle: {type: Object},
      lineFocusMovement: {type: Object},
    };
  }

  // Reactive properties below
  accessor presentationState: number = 0;
  accessor availableVoices: SpeechSynthesisVoice[] = [];
  accessor enabledLangs: string[] = [];
  // If Read Aloud is playing speech.
  accessor isSpeechActive: boolean = false;
  // If speech is actually playing. Due to latency with the TTS engine, there
  // can be a delay between when the user presses play and speech actually
  // plays.
  accessor isAudioCurrentlyPlaying: boolean = false;
  // If Read Aloud is playable. Certain states, such as when Read Anything does
  // not have content or when the speech engine is loading should disable
  // certain toolbar buttons like the play / pause button should be disabled.
  // This is set from the parent element via one way data binding.
  accessor isReadAloudPlayable: boolean = false;
  accessor isReadAnythingPinned: boolean = false;
  accessor localeToDisplayName: {[lang: string]: string} = {};
  accessor previewVoicePlaying: SpeechSynthesisVoice|null = null;
  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor selectedVoice: SpeechSynthesisVoice|undefined;
  accessor pageLanguage: string = '';
  accessor isImmersiveMode: boolean = false;
  accessor lineFocusStyle: LineFocusStyle|null = null;
  accessor lineFocusMovement: LineFocusMovement|null = null;
  protected accessor hideSpinner_: boolean = true;
  protected accessor isImmersiveEnabled_: boolean = false;
  // Overflow buttons on the toolbar that open a menu of options.
  protected accessor moreOptionsButtons_: MenuButton[] = [];
  protected accessor speechRate_: number = 1;
  // Buttons on the toolbar that open a menu of options.
  protected accessor textStyleOptions_: MenuButton[] = [];
  protected accessor textStyleToggles_: ToggleButton[] = [
    {
      id: LINK_TOGGLE_BUTTON_ID,
      icon: chrome.readingMode.linksEnabled?
      LINKS_ENABLED_ICON: LINKS_DISABLED_ICON,
      title: chrome.readingMode.linksEnabled?
           loadTimeData.getString('disableLinksLabel'):
               loadTimeData.getString('enableLinksLabel'),
    },
  ];
  protected accessor areFontsLoaded_: boolean = false;

  // Member variables below
  private startTime_: number = Date.now();
  private constructorTime_: number = 0;
  private currentFocusId_: string = '';
  private windowResizeCallback_: () => void = () => {};
  private toolbarContainerBlurCallback_: () => void = () => {};
  // The previous speech active status so we can track when it changes.
  private wasSpeechActive_: boolean = false;
  private spinnerDebouncerCallbackHandle_?: number;
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  // Corresponds to UI setup being complete on the toolbar when
  // connectedCallback has finished executing.
  private isSetupComplete_: boolean = false;

  isReadingModeInactive(): boolean {
    return this.presentationState ===
        chrome.readingMode.inHiddenPresentationState;
  }

  isReadingModeInSidePanel(): boolean {
    return this.presentationState ===
        chrome.readingMode.inSidePanelPresentationState;
  }

  constructor() {
    super();
    this.constructorTime_ = Date.now();
    this.logger_.logTimeFrom(
        TimeFrom.TOOLBAR, this.startTime_, this.constructorTime_);
    this.isImmersiveEnabled_ = chrome.readingMode.isImmersiveEnabled;

    // Only add the button to the toolbar if the feature is enabled.
    if (chrome.readingMode.imagesFeatureEnabled) {
      this.textStyleToggles_.push({
        id: IMAGES_TOGGLE_BUTTON_ID,
        icon: chrome.readingMode.imagesEnabled ? IMAGES_ENABLED_ICON :
                                                 IMAGES_DISABLED_ICON,
        title: chrome.readingMode.imagesEnabled ?
            loadTimeData.getString('disableImagesLabel') :
            loadTimeData.getString('enableImagesLabel'),
      });
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    this.windowResizeCallback_ = this.maybeUpdateMoreOptions_.bind(this);
    window.addEventListener('resize', this.windowResizeCallback_);
    if (this.isImmersiveEnabled_) {
      this.toolbarContainerBlurCallback_ =
          this.onToolbarContainerBlur_.bind(this);
      this.$.toolbarContainer.addEventListener(
          'blur', this.toolbarContainerBlurCallback_);
    }

    this.loadFontsStylesheet();
    this.initializeMenuButtons_();
    this.isSetupComplete_ = true;
  }

  override disconnectedCallback() {
    if (this.windowResizeCallback_) {
      window.removeEventListener('resize', this.windowResizeCallback_);
    }
    if (this.isImmersiveEnabled_) {
      this.$.toolbarContainer.removeEventListener(
          'blur', this.toolbarContainerBlurCallback_);
    }
    if (this.spinnerDebouncerCallbackHandle_ !== undefined) {
      clearTimeout(this.spinnerDebouncerCallbackHandle_);
    }
    super.disconnectedCallback();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('isSpeechActive') ||
        changedProperties.has('isAudioCurrentlyPlaying')) {
      this.onSpeechPlayingStateChanged_();
    }

    if (changedProperties.has('presentationState') &&
        (this.isReadingModeInSidePanel() || this.isReadingModeInactive())) {
      this.$.toolbarContainer.tabIndex = 0;
      const currentFocusedElement =
          this.$.toolbarContainer.querySelector<HTMLElement>('[tabindex="0"]');
      if (!currentFocusedElement) {
        const tabIndexElementId =
            this.isReadAloudPlayable ? '#play-pause' : '#rate';
        const element = this.$.toolbarContainer.querySelector<HTMLElement>(
            tabIndexElementId);
        if (element) {
          element.tabIndex = 0;
        }
      }
    }
  }

  private maybeUpdateMoreOptions_() {
    if (this.isImmersiveEnabled_) {
      return;
    }

    // Hide the more options button first to calculate if we need it
    const toolbar = this.$.toolbarContainer;
    const moreOptionsButton = toolbar.querySelector<HTMLElement>('#more');
    assert(moreOptionsButton, 'more options button doesn\'t exist');
    this.hideElement_(moreOptionsButton, false);

    // Show all the buttons to see if they fit.
    const buttons =
        Array.from(toolbar.querySelectorAll<HTMLElement>('.text-style-button'));
    assert(buttons, 'no toolbar buttons');
    buttons.forEach(btn => this.showElement_(btn));
    toolbar.dispatchEvent(new CustomEvent('reset-toolbar', {
      bubbles: true,
      composed: true,
    }));

    if (!toolbar.offsetParent) {
      return;
    }

    // When the toolbar's width exceeds the parent width, then the content has
    // overflowed.
    const parentWidth = toolbar.offsetParent.scrollWidth;
    if (toolbar.scrollWidth > parentWidth) {
      // Hide at least 3 buttons and more if needed.
      let numOverflowButtons = 3;
      let nextOverflowButton = buttons[buttons.length - numOverflowButtons];
      assert(nextOverflowButton);
      // No need to hide a button if it only exceeds the width by a little (i.e.
      // only the padding overflows).
      const maxDiff = 5;
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
      this.showElement_(moreOptionsButton);
      const overflowedButtons =
          buttons.slice(buttons.length - numOverflowButtons);
      overflowedButtons.forEach(btn => this.hideElement_(btn, true));
      toolbar.insertBefore(moreOptionsButton, overflowedButtons[0]!);
    }
  }

  private hideElement_(element: HTMLElement, keepSpace: boolean) {
    if (keepSpace) {
      element.classList.add('visibility-hidden');
    } else {
      element.classList.add('hidden');
    }
  }

  private showElement_(element: HTMLElement) {
    element.classList.remove('hidden', 'visibility-hidden');
  }


  private initializeMenuButtons_() {
    const fontSizeElement = {
      id: 'font-size',
      icon: 'read-anything:font-size',
      ariaLabel: loadTimeData.getString('fontSizeTitle'),
      openMenu: (target: HTMLElement) =>
          openMenu(this.$.fontSizeMenu.get(), target),
      announceBlock: html`<div id='size-announce' class='announce-block'
            aria-live='polite'></div>`,
    };
    if (this.isImmersiveEnabled_) {
      this.textStyleOptions_ = [fontSizeElement];
      return;
    }

    this.textStyleOptions_.push(
        fontSizeElement,
        {
          id: 'font',
          icon: 'read-anything:font',
          ariaLabel: loadTimeData.getString('fontNameTitle'),
          openMenu: (target: HTMLElement) => this.$.fontMenu.open(target),
        },
        {
          id: 'color',
          icon: 'read-anything:color',
          ariaLabel: loadTimeData.getString('themeTitle'),
          openMenu: (target: HTMLElement) => this.$.colorMenu.open(target),
        },
        {
          id: 'line-spacing',
          icon: 'read-anything:line-spacing',
          ariaLabel: loadTimeData.getString('lineSpacingTitle'),
          openMenu: (target: HTMLElement) =>
              this.$.lineSpacingMenu.open(target),
        },
        {
          id: 'letter-spacing',
          icon: 'read-anything:letter-spacing',
          ariaLabel: loadTimeData.getString('letterSpacingTitle'),
          openMenu: (target: HTMLElement) =>
              this.$.letterSpacingMenu.open(target),
        },
    );
    if (chrome.readingMode.isLineFocusEnabled) {
      this.textStyleOptions_.push({
        id: 'line-focus',
        icon: 'read-anything:line-focus',
        ariaLabel: loadTimeData.getString('lineFocusLabel'),
        openMenu: (target: HTMLElement) => this.$.lineFocusMenu.open(target),
      });
    }
    this.requestUpdate();
  }

  protected getHighlightButtonLabel_(): string {
    return loadTimeData.getString('voiceHighlightLabel');
  }

  protected getFormattedSpeechRate_(): string {
    const includeSuffix = this.speechRate_ % 1 === 0;
    return includeSuffix ?
        loadTimeData.getStringF(
            'voiceSpeedOptionTitle', this.speechRate_.toLocaleString()) :
        this.speechRate_.toLocaleString();
  }

  protected onCloseClick_() {
    chrome.readingMode.close();
  }

  // Loading the fonts stylesheet can take a while, especially with slow
  // Internet connections. Since we don't want this to block the rest of
  // Reading Mode from loading, we load this stylesheet asynchronously
  // in TypeScript instead of in read_anything.html
  loadFontsStylesheet() {
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
    }, {once: true});

    document.head.appendChild(link);
  }

  setFontsLoaded() {
    this.areFontsLoaded_ = true;
  }

  protected onResetToolbar_() {
    this.$.moreOptionsMenu.getIfExists()?.close();
    this.moreOptionsButtons_ = [];
    this.style.setProperty('--toolbar-flex-wrap', flexWrapTypical);
  }

  protected onToolbarOverflow_(
      event:
          CustomEvent<{numOverflowButtons: number, overflowLength: number}>) {
    const firstHiddenButton =
        this.textStyleOptions_.length - event.detail.numOverflowButtons;
    // Wrap the buttons if we overflow significantly but aren't yet scrolling
    // the whole app.
    if (firstHiddenButton < 0 &&
        event.detail.overflowLength < minOverflowLengthToScroll) {
      this.style.setProperty('--toolbar-flex-wrap', flexWrapOverflow);
      return;
    }

    // If we only overflow by a little, use the more options button.
    this.moreOptionsButtons_ = this.textStyleOptions_.slice(firstHiddenButton);
  }

  restoreSettingsFromPrefs() {
    this.updateLinkToggleButton();
    this.updateImagesToggleButton();
    this.setFont_(chrome.readingMode.fontName);
    this.speechRate_ = getCurrentSpeechRate();
  }

  protected playPauseButtonAriaLabel_() {
    return loadTimeData.getString('playAriaLabel');
  }

  protected playPauseButtonTitle_() {
    return loadTimeData.getString(
        this.isSpeechActive ? 'pauseTooltip' : 'playTooltip');
  }

  protected playPauseButtonIronIcon_() {
    return this.isSpeechActive ? 'read-anything-20:pause' :
                                 'read-anything-20:play';
  }

  protected onNextGranularityClick_() {
    this.logger_.logSpeechControlClick(SpeechControls.NEXT);
    this.fire(ToolbarEvent.NEXT_GRANULARITY);
  }

  protected onPreviousGranularityClick_() {
    this.logger_.logSpeechControlClick(SpeechControls.PREVIOUS);
    this.fire(ToolbarEvent.PREVIOUS_GRANULARITY);
  }

  protected onTextStyleMenuButtonFromOverflowClick_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number.parseInt(currentTarget.dataset['index']!);
    const menu = this.moreOptionsButtons_[index];
    assert(menu);
    menu.openMenu(currentTarget);
  }

  protected onTextStyleMenuButtonClick_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number.parseInt(currentTarget.dataset['index']!);
    const menu = this.textStyleOptions_[index];
    assert(menu);
    menu.openMenu(currentTarget);
  }

  protected onShowRateMenuClick_(event: MouseEvent) {
    this.$.rateMenu.open(event.target as HTMLElement);
  }

  protected onVoiceSelectionMenuClick_(event: MouseEvent) {
    const voiceMenu =
        this.$.toolbarContainer.querySelector('#voiceSelectionMenu');
    assert(voiceMenu, 'no voiceMenu element');
    (voiceMenu as VoiceSelectionMenuElement)
        .onVoiceSelectionMenuClick(event.target as HTMLElement);
  }

  protected onMoreOptionsClick_(event: MouseEvent) {
    const target = event.target as HTMLElement;
    if (this.isImmersiveEnabled_) {
      this.$.settingsMenu.open(target);
      return;
    }

    const menu = this.$.moreOptionsMenu.get();
    openMenu(menu, target);
  }

  protected onHighlightChange_(event: CustomEvent<{data: number}>) {
    // Event handler for highlight-change (from highlight-menu).
    const changedHighlight = event.detail.data;
    if (!this.isImmersiveEnabled_) {
      this.setHighlightButtonIcon_(
          changedHighlight !== chrome.readingMode.noHighlighting);
    }
  }

  protected onHighlightClick_(event: MouseEvent) {
    // Click handler for the highlight button. Used both for the
    // highlight menu mode and the toggle button mode.
    this.$.highlightMenu.open(event.target as HTMLElement);
  }

  private setHighlightButtonIcon_(turnOn: boolean) {
    // Sets the icon of the highlight button. This happens regardless of
    // whether the button toggles highlight on/off (the behavior when the phrase
    // highlighting flag is off), or the button shows the highlight menu (when
    // the flag is on).
    const button = this.$.toolbarContainer.querySelector('#highlight');
    assert(button, 'no highlight button');
    if (turnOn) {
      button.setAttribute('iron-icon', 'read-anything:highlight-on');
    } else {
      button.setAttribute('iron-icon', 'read-anything:highlight-off');
    }
  }

  private setFont_(font: string) {
    this.style.fontFamily = chrome.readingMode.getValidatedFontName(font);
  }

  protected onFontChange_(event: CustomEvent<{data: string}>) {
    this.setFont_(event.detail.data);
  }

  protected onRateChange_(event: CustomEvent<{data: number}>) {
    this.speechRate_ = event.detail.data;
  }

  protected onFontSizeIncreaseClick_() {
    this.updateFontSize_(true);
  }

  protected onFontSizeDecreaseClick_() {
    this.updateFontSize_(false);
  }

  protected onToggleButtonClick_(e: Event) {
    const toggleMenuId = (e.currentTarget as HTMLElement).id;

    if (toggleMenuId === LINK_TOGGLE_BUTTON_ID) {
      this.onToggleLinksClick_();
    } else if (toggleMenuId === IMAGES_TOGGLE_BUTTON_ID) {
      this.onToggleImagesClick_();
    }
  }

  private onToggleLinksClick_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINKS_ENABLED_CHANGE);

    chrome.readingMode.onLinksEnabledToggled();
    this.fire(ToolbarEvent.LINKS);
    this.updateLinkToggleButton();
  }

  private onToggleImagesClick_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.IMAGES_ENABLED_CHANGE);

    chrome.readingMode.onImagesEnabledToggled();
    this.fire(ToolbarEvent.IMAGES);
    this.updateImagesToggleButton();
  }

  private updateLinkToggleButton() {
    const button = this.shadowRoot.getElementById(LINK_TOGGLE_BUTTON_ID) as
        CrIconButtonElement;
    if (button) {
      button.ironIcon = chrome.readingMode.linksEnabled ? LINKS_ENABLED_ICON :
                                                          LINKS_DISABLED_ICON;
      const linkStatusLabel = chrome.readingMode.linksEnabled ?
          loadTimeData.getString('disableLinksLabel') :
          loadTimeData.getString('enableLinksLabel');
      button.title = linkStatusLabel;
      button.ariaLabel = linkStatusLabel;
    }
  }

  private updateImagesToggleButton() {
    const button = this.shadowRoot?.getElementById(IMAGES_TOGGLE_BUTTON_ID) as
        CrIconButtonElement;
    if (button) {
      button.ironIcon = chrome.readingMode.imagesEnabled ? IMAGES_ENABLED_ICON :
                                                           IMAGES_DISABLED_ICON;
      const imageStatusLabel = chrome.readingMode.imagesEnabled ?
          loadTimeData.getString('disableImagesLabel') :
          loadTimeData.getString('enableImagesLabel');
      button.title = imageStatusLabel;
      button.ariaLabel = imageStatusLabel;
    }
  }

  private announceSizeChage(increase: boolean) {
    const sizeChangeAnnounce: HTMLDivElement =
        this.shadowRoot?.getElementById('size-announce') as HTMLDivElement;
    if (sizeChangeAnnounce) {
      // We must add a new HTML element otherwise aria-live won't catch it.
      const paragraph: HTMLParagraphElement = document.createElement('p');
      if (increase) {
        paragraph.textContent = this.i18n('increaseFontSizeAnnouncement');
      } else {
        paragraph.textContent = this.i18n('decreaseFontSizeAnnouncement');
      }
      sizeChangeAnnounce.appendChild(paragraph);
      // To avoid adding indefinite number of HTML elements. If the list of
      // paragraphs in size_change_announce has become too large reset it.
      if (sizeChangeAnnounce.getElementsByTagName('p').length >
          MAX_PARAGRAPHS_IN_ANNOUNCE_BLOCK) {
        this.restoreAnnounceState('size-announce');
      }
    }
  }


  // Helper function to clear html in an aria announce element.
  private restoreAnnounceState(id: string) {
    const srNotice: HTMLElement|null = this.shadowRoot?.getElementById(id);
    if (srNotice) {
      const paragraphs = srNotice.querySelectorAll('p');
      paragraphs.forEach(paragraph => {
        paragraph.remove();
      });
    }
  }

  private updateFontSize_(increase: boolean) {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.FONT_SIZE_CHANGE);
    const startingSize = chrome.readingMode.fontSize;
    chrome.readingMode.onFontSizeChanged(increase);
    this.fire(ToolbarEvent.FONT_SIZE);
    if (startingSize !== chrome.readingMode.fontSize) {
      this.announceSizeChage(increase);
    }
    // Don't close the menu
  }

  protected onFontResetClick_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.FONT_SIZE_CHANGE);
    chrome.readingMode.onFontSizeReset();
    this.fire(ToolbarEvent.FONT_SIZE);
  }

  protected onPlayPauseClick_() {
    this.logger_.logSpeechControlClick(
        this.isSpeechActive ? SpeechControls.PAUSE : SpeechControls.PLAY);
    if (this.isSpeechActive) {
      this.logger_.logSpeechStopSource(
          chrome.readingMode.pauseButtonStopSource);
    }
    this.fire(ToolbarEvent.PLAY_PAUSE);
  }

  protected onToolbarKeydown_(e: KeyboardEvent) {
    const toolbar = this.$.toolbarContainer;
    const buttons =
        Array.from(toolbar.querySelectorAll<HTMLElement>('.toolbar-button'));
    assert(buttons, 'no toolbar buttons');

    // Only allow focus on the currently visible and actionable elements.
    const focusableElements = buttons.filter(el => {
      return (el.clientHeight > 0) && (el.clientWidth > 0) &&
          (el.getBoundingClientRect().right < toolbar.clientWidth) &&
          (el.style.visibility !== 'hidden') && (el.style.display !== 'none') &&
          (!(el as any).disabled) && (el.className !== 'separator');
    });

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

  protected onFontSizeMenuKeydown_(e: KeyboardEvent) {
    // The font size selection menu is laid out horizontally, so users should be
    // able to navigate it using either up and down arrows, or left and right
    // arrows.
    if (!isArrow(e.key)) {
      return;
    }
    e.preventDefault();
    const focusableElements =
        Array.from(this.$.fontSizeMenu.get().children) as HTMLElement[];
    assert(e.target instanceof HTMLElement);
    const elementToFocus =
        focusableElements[getNewIndex(e.key, e.target, focusableElements)];
    assert(elementToFocus, 'no element to focus');
    elementToFocus.focus();
  }

  protected onCloseAllMenus_(
      event: CustomEvent<{previousId: SettingsOption | null}>) {
    this.closeAllMenus_(event.detail?.previousId);
  }

  protected onCloseSubmenuRequested_(
      event: CustomEvent<{previousId: SettingsOption}>) {
    this.closeSubmenu_(event.detail.previousId);
  }

  protected onOpenSettingsSubmenu_(event: CustomEvent<{
    id: SettingsOption,
    previousId: SettingsOption|null,
    target: HTMLElement,
  }>) {
    if (!this.isImmersiveEnabled_) {
      return;
    }

    const {id, previousId, target} = event.detail;
    if (previousId) {
      const previousMenu = this.settingsMenu_[previousId];
      previousMenu?.close();
    }

    const showAtConfig = {
      minY: 0,
      anchorAlignmentX: AnchorAlignment.BEFORE_START,
      anchorAlignmentY: AnchorAlignment.AFTER_START,
    };
    const currentMenu = this.settingsMenu_[id];
    currentMenu?.open(target, showAtConfig);
  }

  private closeAllMenus_(previousId: SettingsOption|null = null) {
    if (!this.isImmersiveEnabled_) {
      return;
    }

    if (previousId) {
      this.closeSubmenu_(previousId);
    }

    this.$.settingsMenu.close();
  }

  private closeSubmenu_(submenuId: SettingsOption) {
    const previousMenu = this.settingsMenu_[submenuId];
    assert(previousMenu, `settings ${submenuId} submenu not found`);
    previousMenu.close();
  }

  get settingsMenu_(): Partial<Record<SettingsOption, ToolbarMenu>> {
    return {
      [SettingsOption.COLOR]: this.$.colorMenu,
      [SettingsOption.VOICE_HIGHLIGHT]: this.$.highlightMenu,
      [SettingsOption.FONT]: this.$.fontMenu,
      [SettingsOption.LETTER_SPACING]: this.$.letterSpacingMenu,
      [SettingsOption.LINE_FOCUS]: this.$.lineFocusMenu,
      [SettingsOption.LINE_SPACING]: this.$.lineSpacingMenu,
      [SettingsOption.VOICE_SELECTION]: this.$.voiceSelectionMenu,
      [SettingsOption.PRESENTATION]: this.$.presentationMenu,
    };
  }

  private getMoreOptionsButtons_(): HTMLElement[] {
    return Array.from(
        this.$.toolbarContainer.querySelectorAll(moreOptionsClass));
  }

  private onKeyDown_(e: KeyboardEvent, focusableElements: HTMLElement[]) {
    if (!isHorizontalArrow(e.key)) {
      return;
    }

    e.preventDefault();
    //  Move to the next focusable item in the toolbar, wrapping around
    //  if we've reached the end or beginning.
    assert(e.target instanceof HTMLElement);
    let newIndex = getNewIndex(e.key, e.target, focusableElements);
    const direction = isForwardArrow(e.key) ? 1 : -1;
    // If the next item has overflowed, skip focusing the more options button
    // itself and go directly to the children. We still need this button in the
    // list of focusable elements because it can become focused by tabbing while
    // the menu is open and we want the arrow key behavior to continue smoothly.
    // This is skipped in immersive mode because the more options button opens
    // the main settings menu instead of the overflow menu.
    const elementToFocus = focusableElements[newIndex];
    assert(elementToFocus);
    if ((elementToFocus.id === 'more' && !this.isImmersiveEnabled_) ||
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

  private resetHideSpinnerDebouncer_() {
    // Use a debouncer to reduce glitches. Even when audio is fast to respond to
    // the play button, there are still milliseconds of delay. To prevent the
    // spinner from quickly appearing and disappearing, we use a debouncer. If
    // either the values of `isSpeechActive` or `isAudioCurrentlyPlaying`
    // change, the previously scheduled callback is canceled and a new callback
    // is scheduled.
    // TODO: crbug.com/339860819 - improve debouncer logic so that the spinner
    // disappears immediately when speech starts playing, or when the pause
    // button is hit.
    if (this.spinnerDebouncerCallbackHandle_ !== undefined) {
      clearTimeout(this.spinnerDebouncerCallbackHandle_);
    }
    this.spinnerDebouncerCallbackHandle_ = setTimeout(() => {
      this.hideSpinner_ = !this.isSpeechActive || this.isAudioCurrentlyPlaying;
      this.spinnerDebouncerCallbackHandle_ = undefined;
    }, spinnerDebounceTimeout);
  }

  private onSpeechPlayingStateChanged_() {
    this.resetHideSpinnerDebouncer_();

    // If the previously focused item becomes disabled or disappears from the
    // toolbar because of speech starting or stopping, put the focus on the
    // play/pause button so keyboard navigation continues working.
    // If we're still loading the reading mode panel on
    // a first open, we shouldn't attempt to refocus the play button or the
    // rate menu.
    if (this.isSetupComplete_ && (this.shadowRoot !== null) &&
        (this.shadowRoot.activeElement === null ||
         this.shadowRoot.activeElement.clientHeight === 0)) {
      // If the play / pause button is enabled, we should focus it. Otherwise,
      // we should focus the rate menu.
      const tagToFocus = this.isReadAloudPlayable ? '#play-pause' : '#rate';
      this.$.toolbarContainer.querySelector<HTMLElement>(tagToFocus)?.focus();
    }

    if (this.isSpeechActive !== this.wasSpeechActive_) {
      this.maybeUpdateMoreOptions_();
      this.wasSpeechActive_ = this.isSpeechActive;
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
      if (!this.isImmersiveEnabled_) {
        this.$.moreOptionsMenu.getIfExists()?.close();
      }
    }

    // Wait for the next animation frame for the overflow menu to show or hide.
    requestAnimationFrame(() => {
      elementToFocus.focus();
    });
  }

  protected getRateTabIndex_(): number {
    return (!this.isReadAloudPlayable || this.currentFocusId_ === 'rate') ? 0 :
                                                                            -1;
  }

  protected getVoiceSpeedLabel_(): string {
    return loadTimeData.getStringF('voiceSpeedWithRateLabel', this.speechRate_);
  }

  protected getAudioState_(): string {
    if (this.isImmersiveEnabled_) {
      return 'immersive-enabled';
    }
    if (this.isSpeechActive) {
      return 'active';
    }
    return 'inactive';
  }

  protected getToolbarContainerClass_(): string {
    return this.isImmersiveEnabled_ ? 'immersive-toolbar-container' :
                                      'toolbar-container';
  }

  protected getGranularityContainerClass_(): string {
    return this.isSpeechActive || this.isImmersiveEnabled_ ?
        'granularity-container-when-active-true' :
        'granularity-container-when-active-false';
  }

  protected shouldDisableGranularityNavButtons_(): boolean {
    return !this.isReadAloudPlayable ||
        (this.isImmersiveEnabled_ && !this.isSpeechActive);
  }

  protected onToolbarContainerBlur_() {
    if (this.isImmersiveEnabled_) {
      this.$.toolbarContainer.tabIndex = -1;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-toolbar': ReadAnythingToolbarElement;
  }
}

customElements.define(
    ReadAnythingToolbarElement.is, ReadAnythingToolbarElement);
