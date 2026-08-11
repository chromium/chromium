// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './grouped_action_menu.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {DEFAULT_SETTINGS, LineFocusMovement, LineFocusStyle, ToolbarEvent} from '../content/read_anything_types.js';
import type {SettingsPrefs, ShowAtConfigPrefs} from '../content/read_anything_types.js';
import {ReadAnythingSettingsChange} from '../shared/metrics_browser_proxy.js';
import {ReadAnythingLogger} from '../shared/read_anything_logger.js';

import type {GroupedActionMenuElement} from './grouped_action_menu.js';
import {SettingsItemType} from './menu_util.js';
import type {MenuGroup, MenuStateItem, ToolbarMenu} from './menu_util.js';
import {getHtml} from './text_menu.html.js';

export interface TextMenuElement {
  $: {
    menu: GroupedActionMenuElement,
  };
}

const TextMenuElementBase = WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export const MAX_EXPANDED_FONT_COUNT = 3;

export class TextMenuElement extends TextMenuElementBase implements
    ToolbarMenu {
  static get is() {
    return 'text-menu';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      settingsPrefs: {type: Object},
      nonModal: {type: Boolean},
      areFontsLoaded: {type: Boolean},
      pageLanguage: {type: String},
      groups_: {type: Array},
      isFontMenuExpanded: {type: Boolean},
      lineFocusStyle: {type: Object},
      lineFocusEnabled: {type: Boolean},
      lineFocusMovement: {type: Number},
    };
  }

  accessor settingsPrefs: SettingsPrefs = DEFAULT_SETTINGS;
  accessor nonModal: boolean = false;
  accessor areFontsLoaded: boolean = false;
  accessor pageLanguage: string = '';
  accessor isFontMenuExpanded: boolean = false;
  accessor lineFocusStyle: LineFocusStyle|null = null;
  accessor lineFocusEnabled: boolean = false;
  accessor lineFocusMovement: LineFocusMovement|null = null;

  private fontOptions_: Array<MenuStateItem<string>> = [];
  private lineSpacingOptions_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('lineSpacingStandardTitle'),
      icon: 'read-anything:line-spacing-standard-custom',
      data: chrome.readingMode.standardLineSpacing,
    },
    {
      title: loadTimeData.getString('lineSpacingLooseTitle'),
      icon: 'read-anything:line-spacing-loose-custom',
      data: chrome.readingMode.looseLineSpacing,
    },
    {
      title: loadTimeData.getString('lineSpacingVeryLooseTitle'),
      icon: 'read-anything:line-spacing-very-loose-custom',
      data: chrome.readingMode.veryLooseLineSpacing,
    },
  ];

  private letterSpacingOptions_: Array<MenuStateItem<number>> = [
    {
      title: loadTimeData.getString('letterSpacingStandardTitle'),
      icon: loadTimeData.getBoolean('webuiRoundedIconsEnabled')?
      'read-anything:format-letter-spacing-standard':
          'read-anything:letter-spacing-standard-old',
      data: chrome.readingMode.standardLetterSpacing,
    },
    {
      title: loadTimeData.getString('letterSpacingWideTitle'),
      icon: loadTimeData.getBoolean('webuiRoundedIconsEnabled')?
      'read-anything:format-letter-spacing-wide':
          'read-anything:letter-spacing-wide-old',
      data: chrome.readingMode.wideLetterSpacing,
    },
    {
      title: loadTimeData.getString('letterSpacingVeryWideTitle'),
      icon: loadTimeData.getBoolean('webuiRoundedIconsEnabled')?
      'read-anything:format-letter-spacing-wider':
          'read-anything:letter-spacing-very-wide-old',
      data: chrome.readingMode.veryWideLetterSpacing,
    },
  ];

  private toggleOptions_: Array<MenuStateItem<boolean>> = [
    {
      title: loadTimeData.getString('lineFocusOffTitle'),
      data: false,
    },
    {
      title: loadTimeData.getString('lineFocusOnTitle'),
      data: true,
    },
  ];

  private styleOptions_: Array<MenuStateItem<LineFocusStyle>> = [
    {
      title: loadTimeData.getString('lineFocusUnderlineTitle'),
      data: LineFocusStyle.UNDERLINE,
    },
    {
      title: loadTimeData.getString('lineFocusOneLineTitle'),
      data: LineFocusStyle.SMALL_WINDOW,
    },
    {
      title: loadTimeData.getString('lineFocusThreeLineTitle'),
      data: LineFocusStyle.MEDIUM_WINDOW,
    },
    {
      title: loadTimeData.getString('lineFocusFiveLineTitle'),
      data: LineFocusStyle.LARGE_WINDOW,
    },
  ];

  private movementOptions_: Array<MenuStateItem<LineFocusMovement>> = [
    {
      title: loadTimeData.getString('lineFocusStaticTitle'),
      data: LineFocusMovement.STATIC,
    },
    {
      title: loadTimeData.getString('lineFocusCursorLineTitle'),
      data: LineFocusMovement.CURSOR,
    },
  ];

  private get defaultGroups_():
      Array<MenuGroup<number|string|boolean|LineFocusStyle|LineFocusMovement>> {
    return [
      {
        header: {
          title: loadTimeData.getString('fontNameTitle'),
          separator: false,
        },
        items: this.fontOptions_,
        eventName: ToolbarEvent.FONT,
      },
      {
        header: {
          title: loadTimeData.getString('lineSpacingTitle'),
          separator: true,
        },
        items: this.lineSpacingOptions_,
        eventName: ToolbarEvent.LINE_SPACING,
      },
      {
        header: {
          title: loadTimeData.getString('letterSpacingTitle'),
          separator: true,
        },
        items: this.letterSpacingOptions_,
        eventName: ToolbarEvent.LETTER_SPACING,
      },
    ];
  }

  protected accessor groups_:
      Array<MenuGroup<number|string|boolean|LineFocusStyle|LineFocusMovement>> =
          this.defaultGroups_;
  private logger_: ReadAnythingLogger = ReadAnythingLogger.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (chrome.readingMode.isLineFocusEnabled) {
      if (changedProperties.has('lineFocusEnabled')) {
        this.updateOptionsForToggle_(this.lineFocusEnabled);
      }
      if (changedProperties.has('lineFocusStyle') &&
          this.lineFocusStyle !== null) {
        this.updateOptionsForStyle_(this.lineFocusStyle);
      }
      if (changedProperties.has('lineFocusMovement') &&
          this.lineFocusMovement !== null) {
        this.updateOptionsForMovement_(this.lineFocusMovement);
      }
    }

    if (changedProperties.has('settingsPrefs') ||
        changedProperties.has('pageLanguage') ||
        changedProperties.has('areFontsLoaded') ||
        changedProperties.has('isFontMenuExpanded') ||
        changedProperties.has('lineFocusEnabled') ||
        changedProperties.has('lineFocusStyle') ||
        changedProperties.has('lineFocusMovement')) {
      this.computeFontOptions_();
      this.updateOptionsForFont_();
      this.updateOptionsForLineSpacing_();
      this.updateOptionsForLetterSpacing_();
      this.computeGroups_();
      this.groups_ = [...this.groups_];
    }
  }

  open(anchor: HTMLElement, showAtConfig?: ShowAtConfigPrefs) {
    this.$.menu.open(anchor, showAtConfig);
  }

  close() {
    this.$.menu.close();
    this.isFontMenuExpanded = false;
  }

  protected onFontChange_(event: CustomEvent<{data: string}>) {
    const newFont = event.detail.data;
    if (newFont === ToolbarEvent.EXPAND_FONTS_SENTINEL) {
      event.stopImmediatePropagation();
      this.isFontMenuExpanded = true;
      return;
    }
    chrome.readingMode.onFontChange(newFont);
    this.logger_.logTextSettingsChange(ReadAnythingSettingsChange.FONT_CHANGE);
  }

  protected onLineSpacingChange_(event: CustomEvent<{data: number}>) {
    const newSpacing = event.detail.data;
    chrome.readingMode.onLineSpacingChange(newSpacing);
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_HEIGHT_CHANGE);
  }

  protected onLetterSpacingChange_(event: CustomEvent<{data: number}>) {
    const newSpacing = event.detail.data;
    chrome.readingMode.onLetterSpacingChange(newSpacing);
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LETTER_SPACING_CHANGE);
  }

  protected onLineFocusStyleChange_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_STYLE_CHANGE);
  }

  protected onLineFocusToggleChange_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_TOGGLE);
  }

  protected onLineFocusMovementChange_() {
    this.logger_.logTextSettingsChange(
        ReadAnythingSettingsChange.LINE_FOCUS_MOVEMENT_CHANGE);
  }

  private updateOptionsForFont_() {
    const currentFont = chrome.readingMode.fontName;
    let hasSelected = false;
    this.fontOptions_.forEach(option => {
      option.selected = option.data === currentFont;
      if (option.selected) {
        hasSelected = true;
      }
    });
    if (!hasSelected && this.fontOptions_.length > 0) {
      if (this.fontOptions_[0]) {
        this.fontOptions_[0].selected = true;
      }
    }
  }

  private updateOptionsForLineSpacing_() {
    const currentSpacing = this.settingsPrefs.lineSpacing;
    this.lineSpacingOptions_.forEach(option => {
      option.selected = option.data === currentSpacing;
    });
  }

  private updateOptionsForLetterSpacing_() {
    const currentSpacing = this.settingsPrefs.letterSpacing;
    this.letterSpacingOptions_.forEach(option => {
      option.selected = option.data === currentSpacing;
    });
  }

  private updateOptionsForToggle_(isEnabled: boolean) {
    this.toggleOptions_.forEach(option => {
      option.selected = option.data === isEnabled;
    });
  }

  private updateOptionsForStyle_(newStyle: LineFocusStyle) {
    this.styleOptions_.forEach(option => {
      option.selected = option.data === newStyle;
    });
  }

  private updateOptionsForMovement_(newMovement: LineFocusMovement) {
    this.movementOptions_.forEach(option => {
      option.selected = option.data === newMovement;
    });
  }

  private computeGroups_() {
    const groups = this.defaultGroups_;

    if (chrome.readingMode.isLineFocusEnabled) {
      groups.push({
        header: {
          title: loadTimeData.getString('lineFocusLabel'),
          separator: true,
        },
        items: this.toggleOptions_,
        eventName: ToolbarEvent.LINE_FOCUS_TOGGLE,
      });

      if (this.lineFocusEnabled) {
        groups.push(
            {
              header: {
                title: loadTimeData.getString('lineFocusStyleHeading'),
                separator: false,
              },
              items: this.styleOptions_,
              eventName: ToolbarEvent.LINE_FOCUS_STYLE,
            },
            {
              header: {
                title: loadTimeData.getString('lineFocusMovementHeading'),
                separator: false,
              },
              items: this.movementOptions_,
              eventName: ToolbarEvent.LINE_FOCUS_MOVEMENT,
            });
      }
    }

    this.groups_ = groups;
  }

  private computeFontOptions_() {
    const fonts = chrome.readingMode.supportedFonts;
    let visibleFonts: string[];
    if (this.isFontMenuExpanded || fonts.length <= MAX_EXPANDED_FONT_COUNT) {
      visibleFonts = fonts;
    } else {
      const currentFont = chrome.readingMode.fontName;
      const currentIndex = fonts.indexOf(currentFont);
      if (currentIndex >= MAX_EXPANDED_FONT_COUNT) {
        // Active font is outside the top 3.
        // Pin first 2 default fonts + active font into the 3 visible slots.
        visibleFonts = [...fonts.slice(0, 2), currentFont];
      } else {
        // Active font is in top 3 (or not set). Show top 3 fonts.
        visibleFonts = fonts.slice(0, 3);
      }
    }
    this.fontOptions_ = visibleFonts.map(
        font => ({
          title: this.areFontsLoaded ?
              font :
              `${font}\u00A0${this.i18n('readingModeFontLoadingText')}`,
          data: font,
          style: `font-family:${font}`,
          itemType: SettingsItemType.RADIO,
        }));
    if (!this.isFontMenuExpanded && fonts.length > MAX_EXPANDED_FONT_COUNT) {
      this.fontOptions_.push({
        title: loadTimeData.getString('moreOptionsLabel'),
        data: ToolbarEvent.EXPAND_FONTS_SENTINEL,
        itemType: SettingsItemType.EXPAND,
      });
    }
    this.updateOptionsForFont_();
    if (this.groups_[0]) {
      this.groups_[0].items = this.fontOptions_;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'text-menu': TextMenuElement;
  }
}

customElements.define(TextMenuElement.is, TextMenuElement);
