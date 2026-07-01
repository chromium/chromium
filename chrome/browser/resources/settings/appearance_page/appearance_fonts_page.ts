// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_slider.js';
import '../settings_page/settings_subpage.js';
import '../controls/settings_dropdown_menu.js';

import type {FontsBrowserProxy, FontsData} from '/shared/settings/appearance_page/fonts_browser_proxy.js';
import {FontsBrowserProxyImpl} from '/shared/settings/appearance_page/fonts_browser_proxy.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {SliderTick} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';

import {getCss} from './appearance_fonts_page.css.js';
import {getHtml} from './appearance_fonts_page.html.js';

const FONT_SIZE_RANGE: number[] = [
  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
  26, 28, 30, 32, 34, 36, 40, 44, 48, 56, 64, 72,
];

const MINIMUM_FONT_SIZE_RANGE: number[] =
    [0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24];

function ticksWithLabels(ticks: number[]): SliderTick[] {
  return ticks.map(x => ({label: `${x}`, value: x, ariaValue: undefined}));
}

/**
 * 'settings-appearance-fonts-page' is the settings page containing appearance
 * settings.
 */

export interface SettingsAppearanceFontsPageElement {
  $: {
    fixedFontPreview: HTMLElement,
    mathFontPreview: HTMLElement,
    minimumSizeFontPreview: HTMLElement,
    sansSerifFontPreview: HTMLElement,
    serifFontPreview: HTMLElement,
    standardFontPreview: HTMLElement,
  };
}

const SettingsAppearanceFontsPageElementBase =
    SettingsViewMixinLit(PrefServiceObserverMixinLit(CrLitElement));

export class SettingsAppearanceFontsPageElement extends
    SettingsAppearanceFontsPageElementBase {
  static get is() {
    return 'settings-appearance-fonts-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      fontOptions_: {type: Array},
      fontSizeRange_: {type: Array},
      minimumFontSizeRange_: {type: Array},
      defaultFixedFontSizePref_: {type: Object},
      defaultFontSizePref_: {type: Object},
      fixedFontPref_: {type: Object},
      mathFontPref_: {type: Object},
      minimumFontSizePref_: {type: Object},
      sansSerifFontPref_: {type: Object},
      serifFontPref_: {type: Object},
      standardFontPref_: {type: Object},
    };
  }

  protected accessor fontOptions_: DropdownMenuOptionList = [];
  protected accessor fontSizeRange_: SliderTick[] =
      ticksWithLabels(FONT_SIZE_RANGE);
  protected accessor minimumFontSizeRange_: SliderTick[] =
      ticksWithLabels(MINIMUM_FONT_SIZE_RANGE);
  protected accessor defaultFixedFontSizePref_:
      chrome.settingsPrivate.PrefObject<number>|undefined;
  protected accessor defaultFontSizePref_:
      chrome.settingsPrivate.PrefObject<number>|undefined;
  protected accessor fixedFontPref_: chrome.settingsPrivate.PrefObject<string>|
      undefined;
  protected accessor mathFontPref_: chrome.settingsPrivate.PrefObject<string>|
      undefined;
  protected accessor minimumFontSizePref_:
      chrome.settingsPrivate.PrefObject<number>|undefined;
  protected accessor sansSerifFontPref_:
      chrome.settingsPrivate.PrefObject<string>|undefined;
  protected accessor serifFontPref_: chrome.settingsPrivate.PrefObject<string>|
      undefined;
  protected accessor standardFontPref_:
      chrome.settingsPrivate.PrefObject<string>|undefined;

  private browserProxy_: FontsBrowserProxy =
      FontsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'webkit.webprefs.default_fixed_font_size': 'defaultFixedFontSizePref_',
      'webkit.webprefs.default_font_size': 'defaultFontSizePref_',
      'webkit.webprefs.fonts.fixed.Zyyy': 'fixedFontPref_',
      'webkit.webprefs.fonts.math.Zyyy': 'mathFontPref_',
      'webkit.webprefs.fonts.sansserif.Zyyy': 'sansSerifFontPref_',
      'webkit.webprefs.fonts.serif.Zyyy': 'serifFontPref_',
      'webkit.webprefs.fonts.standard.Zyyy': 'standardFontPref_',
      'webkit.webprefs.minimum_font_size': 'minimumFontSizePref_',
    });
  }

  override firstUpdated(changedProperties: PropertyValues) {
    super.firstUpdated(changedProperties);
    this.browserProxy_.fetchFontsData().then(this.setFontsData_.bind(this));
  }

  private setFontsData_(response: FontsData) {
    const fontMenuOptions = [];
    for (const fontData of response.fontList) {
      fontMenuOptions.push({value: fontData[0], name: fontData[1]});
    }
    this.fontOptions_ = fontMenuOptions;
  }

  /**
   * Get the minimum font size, accounting for unset prefs.
   */
  protected getMinimumFontSize_(): number {
    return this.minimumFontSizePref_?.value || MINIMUM_FONT_SIZE_RANGE[0];
  }

  protected getMinimumSizeHidden_(): boolean {
    return this.minimumFontSizePref_ === undefined ||
        this.minimumFontSizePref_.value <= 0;
  }

  protected fontFamilyValueForFixed_(): string {
    if (!this.fixedFontPref_) {
      return '';
    }
    // <if expr="is_macosx">
    // Osaka font family, which is bundled with macOS, contains a proportional
    // and a fixed-width fonts. The CSS `font-family` property distinguishes
    // them by assuming 'Osaka' for the proportional font and 'Osaka-Mono' for
    // the fixed-width font. See crbug.com/40535332.
    if (this.fixedFontPref_.value === 'Osaka') {
      return 'Osaka-Mono';
    }
    // </if>
    return this.fixedFontPref_.value;
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-appearance-fonts-page': SettingsAppearanceFontsPageElement;
  }
}

customElements.define(
    SettingsAppearanceFontsPageElement.is, SettingsAppearanceFontsPageElement);
