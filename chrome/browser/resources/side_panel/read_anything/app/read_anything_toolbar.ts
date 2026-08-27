// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '../read_aloud/voice_selection_menu.js';
import '../menus/simple_action_menu.js';
import '../menus/appearance_menu.js';
import '../menus/audio_menu.js';
import '../menus/media_menu.js';
import '../menus/color_menu.js';
import '../menus/font_menu.js';
import '../menus/line_focus_menu.js';
import '../menus/line_spacing_menu.js';
import '../menus/letter_spacing_menu.js';
import '../menus/text_menu.js';
import '../menus/highlight_menu.js';
import '../menus/rate_menu.js';
import '../menus/presentation_menu.js';
import '../menus/settings_menu.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/icons.html.js';

import {HelpBubbleMixinLit} from '//resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {LineFocusMovement, LineFocusStyle, SettingsPrefs} from '../content/read_anything_types.js';
import {DEFAULT_SETTINGS, SettingsOption, ToolbarEvent} from '../content/read_anything_types.js';
import type {AppearanceMenuElement} from '../menus/appearance_menu.js';
import type {AudioMenuElement} from '../menus/audio_menu.js';
import type {ColorMenuElement} from '../menus/color_menu.js';
import type {FontMenuElement} from '../menus/font_menu.js';
import type {HighlightMenuElement} from '../menus/highlight_menu.js';
import type {LetterSpacingMenuElement} from '../menus/letter_spacing_menu.js';
import type {LineFocusMenuElement} from '../menus/line_focus_menu.js';
import type {LineSpacingMenuElement} from '../menus/line_spacing_menu.js';
import type {MediaMenuElement} from '../menus/media_menu.js';
import type {ToolbarMenu} from '../menus/menu_util.js';
import type {PresentationMenuElement} from '../menus/presentation_menu.js';
import type {RateMenuElement} from '../menus/rate_menu.js';
import type {SettingsMenuElement} from '../menus/settings_menu.js';
import type {TextMenuElement} from '../menus/text_menu.js';
import type {AudioBrowserProxy} from '../read_aloud/audio_browser_proxy.js';
import {AudioBrowserProxyImpl} from '../read_aloud/audio_browser_proxy.js';
import {getCurrentSpeechRate} from '../read_aloud/speech_presentation_rules.js';
import type {VoiceSelectionMenuElement} from '../read_aloud/voice_selection_menu.js';
import {minOverflowLengthToScroll, openMenu, spinnerDebounceTimeout} from '../shared/common.js';
import {getNewIndex, isArrow, isHorizontalArrow} from '../shared/keyboard_util.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger, SpeechControls, TimeFrom} from '../shared/read_anything_logger.js';

import {getCss} from './read_anything_toolbar.css.js';
import {getHtml} from './read_anything_toolbar.html.js';
import type {VisualBrowserProxy} from './visual_browser_proxy.js';
import {VisualBrowserProxyImpl} from './visual_browser_proxy.js';

export interface ReadAnythingToolbarElement {
  $: {
    rateMenu: RateMenuElement,
    appearanceMenu: AppearanceMenuElement,
    audioMenu: AudioMenuElement,
    mediaMenu: MediaMenuElement,
    colorMenu: ColorMenuElement,
    lineSpacingMenu: LineSpacingMenuElement,
    letterSpacingMenu: LetterSpacingMenuElement,
    fontMenu: FontMenuElement,
    textMenu: TextMenuElement,
    fontSizeMenu: CrLazyRenderLitElement<CrActionMenuElement>,
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
  announceId?: string;
}

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
    HelpBubbleMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement)));

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
      hideSpinner_: {type: Boolean},
      speechRate_: {type: Number},
      pageLanguage: {type: String},
      presentationState: {type: Number},
      isImmersiveMode: {type: Boolean},
      isReadAnythingPinned: {type: Boolean},
      isLineFocusShowing: {type: Boolean},
      lineFocusStyle: {type: Object},
      lineFocusEnabled: {type: Boolean},
      lineFocusMovement: {type: Number},
      webuiRoundedIconsEnabled_: {type: Boolean},
      isAiPlaybackActive: {type: Boolean},
      isAiPlaybackUiEnabled_: {type: Boolean},
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
  accessor selectedVoice: SpeechSynthesisVoice|null = null;
  accessor pageLanguage: string = '';
  accessor isImmersiveMode: boolean = false;
  accessor isLineFocusShowing: boolean = false;
  accessor lineFocusStyle: LineFocusStyle|null = null;
  accessor lineFocusEnabled: boolean = false;
  accessor lineFocusMovement: LineFocusMovement|null = null;
  accessor isAiPlaybackActive: boolean = false;
  protected accessor hideSpinner_: boolean = true;
  protected accessor isAiPlaybackUiEnabled_: boolean = false;
  protected accessor speechRate_: number = 1;
  // Buttons on the toolbar that open a menu of options.
  protected accessor textStyleOptions_: MenuButton[] = [];
  protected accessor areFontsLoaded_: boolean = false;
  protected accessor webuiRoundedIconsEnabled_: boolean =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled');

  // Member variables below
  private startTime_: number = Date.now();
  private constructorTime_: number = 0;
  private currentFocusId_: string = '';
  private toolbarContainerBlurCallback_: () => void = () => {};
  // The previous speech active status so we can track when it changes.
  private wasSpeechActive_: boolean = false;
  private spinnerDebouncerCallbackHandle_?: number;
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();
  private visualBrowserProxy_: VisualBrowserProxy =
      VisualBrowserProxyImpl.getInstance();
  private audioBrowserProxy_: AudioBrowserProxy =
      AudioBrowserProxyImpl.getInstance();

  // Corresponds to UI setup being complete on the toolbar when
  // connectedCallback has finished executing.
  private isSetupComplete_: boolean = false;

  protected isFontSizeDefault_(): boolean {
    return this.visualBrowserProxy_.getFontSize() ===
        this.visualBrowserProxy_.getDefaultFontSize();
  }

  isReadingModeInactive(): boolean {
    return this.presentationState ===
        this.visualBrowserProxy_.getInHiddenPresentationState();
  }

  isReadingModeInSidePanel(): boolean {
    return this.presentationState ===
        this.visualBrowserProxy_.getInSidePanelPresentationState();
  }

  constructor() {
    super();
    this.constructorTime_ = Date.now();
    this.logger_.logTimeFrom(
        TimeFrom.TOOLBAR, this.startTime_, this.constructorTime_);
    this.isAiPlaybackUiEnabled_ =
        this.visualBrowserProxy_
            .isReadAnythingReadAloudExperimentalPlaybackUiEnabled();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.toolbarContainerBlurCallback_ =
        this.onToolbarContainerBlur_.bind(this);
    this.$.toolbarContainer.addEventListener(
        'blur', this.toolbarContainerBlurCallback_);

    this.loadFontsStylesheet();
    this.initializeMenuButtons_();
    this.visualBrowserProxy_.restoreSettingsFromPrefs.addListener(
        this.restoreSettingsFromPrefs.bind(this));
    this.isSetupComplete_ = true;
  }

  override disconnectedCallback() {
    this.$.toolbarContainer.removeEventListener(
        'blur', this.toolbarContainerBlurCallback_);
    if (this.spinnerDebouncerCallbackHandle_ !== undefined) {
      clearTimeout(this.spinnerDebouncerCallbackHandle_);
    }
    super.disconnectedCallback();
  }

  override firstUpdated(_changedProperties: PropertyValues) {
    super.firstUpdated(_changedProperties);
    this.registerHelpBubble(
        'kReadAnythingViewModeElementId', '#toolbarContainer');
    this.registerHelpBubble('kReadAnythingSettingsButtonElementId', '#more');
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

  private initializeMenuButtons_() {
    this.textStyleOptions_ = [{
      id: 'font-size',
      icon: loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
          'read-anything:format-size' :
          'read-anything:font-size-old',
      ariaLabel: loadTimeData.getString('fontSizeTitle'),
      openMenu: (target: HTMLElement) =>
          openMenu(this.$.fontSizeMenu.get(), target),
      announceId: 'size-announce',
    }];
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
    this.visualBrowserProxy_.close();
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
    link.href += this.visualBrowserProxy_.getAllFonts().join('|');
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
  }

  restoreSettingsFromPrefs() {
    this.setFont_(this.visualBrowserProxy_.getFontName());
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
    if (this.isSpeechActive) {
      return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
          'read-anything-20:pause-circle-filled' :
          'read-anything-20:pause-old';
    }
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'read-anything-20:play-circle-filled' :
        'read-anything-20:play-old';
  }

  protected onNextGranularityClick_() {
    this.logger_.logSpeechControlClick(SpeechControls.NEXT);
    this.fire(ToolbarEvent.NEXT_GRANULARITY);
  }

  protected onPreviousGranularityClick_() {
    this.logger_.logSpeechControlClick(SpeechControls.PREVIOUS);
    this.fire(ToolbarEvent.PREVIOUS_GRANULARITY);
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

  protected onAiPlaybackClick_() {
    this.isAiPlaybackActive = !this.isAiPlaybackActive;
  }

  protected onMoreOptionsClick_(event: MouseEvent) {
    const target = event.target as HTMLElement;
    this.$.settingsMenu.open(target);
  }

  private setFont_(font: string) {
    this.style.fontFamily = this.visualBrowserProxy_.getValidatedFontName(font);
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
    const startingSize = this.visualBrowserProxy_.getFontSize();
    this.visualBrowserProxy_.onFontSizeChanged(increase);
    this.fire(ToolbarEvent.FONT_SIZE);
    if (startingSize !== this.visualBrowserProxy_.getFontSize()) {
      this.announceSizeChage(increase);
    }
    this.requestUpdate();
    // Don't close the menu
  }

  protected onFontResetClick_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.FONT_SIZE_CHANGE);
    this.visualBrowserProxy_.onFontSizeReset();
    this.fire(ToolbarEvent.FONT_SIZE);
    this.requestUpdate();
  }

  protected onPlayPauseClick_() {
    this.logger_.logSpeechControlClick(
        this.isSpeechActive ? SpeechControls.PAUSE : SpeechControls.PLAY);
    if (this.isSpeechActive) {
      this.logger_.logSpeechStopSource(
          this.audioBrowserProxy_.getPauseButtonStopSource());
    }
    this.fire(ToolbarEvent.PLAY_PAUSE);
  }

  protected onLineFocusOffClick_() {
    this.fire(ToolbarEvent.LINE_FOCUS_TOGGLE, {data: false});
  }

  protected onToolbarKeydown_(e: KeyboardEvent) {
    const toolbar = this.$.toolbarContainer;
    const buttons = Array.from(
        // TODO(crbug.com/342411653): Update the loading spinner to be inside a
        // button and update the querySelectorAll to use just '.toolbar-button'.
        toolbar.querySelectorAll<CrIconButtonElement|CrButtonElement>(
            'cr-icon-button.toolbar-button, cr-button.toolbar-button'));
    assert(buttons, 'no toolbar buttons');

    // Only allow focus on the currently visible and actionable elements.
    const focusableElements: HTMLElement[] = buttons.filter(el => {
      return (el.clientHeight > 0) && (el.clientWidth > 0) &&
          (el.getBoundingClientRect().right < toolbar.clientWidth) &&
          (el.style.visibility !== 'hidden') && (el.style.display !== 'none') &&
          (!el.disabled) && (el.className !== 'separator');
    });

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

  protected onTranslationRequested_() {
    if (!this.visualBrowserProxy_.isReadAnythingTranslateEntryPointEnabled()) {
      return;
    }
    this.visualBrowserProxy_.onTranslationRequested();
  }

  protected onOpenSettingsSubmenu_(event: CustomEvent<{
    id: SettingsOption,
    previousId: SettingsOption|null,
    target: HTMLElement,
  }>) {
    const {id, previousId, target} = event.detail;
    if (previousId) {
      const previousMenu = this.settingsMenu_[previousId];
      previousMenu?.close();
    }

    const showAtConfig = {
      minY: 0,
      anchorAlignmentY: AnchorAlignment.AFTER_START,
    };
    const currentMenu = this.settingsMenu_[id];
    currentMenu?.open(target, showAtConfig);
  }

  private closeAllMenus_(previousId: SettingsOption|null = null) {
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
      [SettingsOption.APPEARANCE]: this.$.appearanceMenu,
      [SettingsOption.AUDIO]: this.$.audioMenu,
      [SettingsOption.MEDIA]: this.$.mediaMenu,
      [SettingsOption.COLOR]: this.$.colorMenu,
      [SettingsOption.VOICE_HIGHLIGHT]: this.$.highlightMenu,
      [SettingsOption.TEXT]: this.$.textMenu,
      [SettingsOption.FONT]: this.$.fontMenu,
      [SettingsOption.LETTER_SPACING]: this.$.letterSpacingMenu,
      [SettingsOption.LINE_FOCUS]: this.$.lineFocusMenu,
      [SettingsOption.LINE_SPACING]: this.$.lineSpacingMenu,
      [SettingsOption.VOICE_SELECTION]: this.$.voiceSelectionMenu,
      [SettingsOption.PRESENTATION]: this.$.presentationMenu,
    };
  }

  private onKeyDown_(e: KeyboardEvent, focusableElements: HTMLElement[]) {
    if (!isHorizontalArrow(e.key)) {
      return;
    }

    e.preventDefault();
    //  Move to the next focusable item in the toolbar, wrapping around
    //  if we've reached the end or beginning.
    assert(e.target instanceof HTMLElement);
    const newIndex = getNewIndex(e.key, e.target, focusableElements);
    const elementToFocus = focusableElements[newIndex];
    assert(elementToFocus);
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
    elementToFocus.tabIndex = 0;

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

  protected shouldDisableGranularityNavButtons_(): boolean {
    return !this.isReadAloudPlayable || !this.isSpeechActive;
  }

  protected onToolbarContainerBlur_() {
    this.$.toolbarContainer.tabIndex = -1;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-toolbar': ReadAnythingToolbarElement;
  }
}

customElements.define(
    ReadAnythingToolbarElement.is, ReadAnythingToolbarElement);
