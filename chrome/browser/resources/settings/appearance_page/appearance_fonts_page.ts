// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_slider.js';
import '../settings_shared.css.js';
import '../controls/settings_dropdown_menu.js';

import type {FontsBrowserProxy, FontsData} from '/shared/settings/appearance_page/fonts_browser_proxy.js';
import {FontsBrowserProxyImpl} from '/shared/settings/appearance_page/fonts_browser_proxy.js';
import type {SliderTick} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';

import {getTemplate} from './appearance_fonts_page.html.js';


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
    minimumSizeFontPreview: HTMLElement,
    sansSerifFontPreview: HTMLElement,
    serifFontPreview: HTMLElement,
    standardFontPreview: HTMLElement,
  };
}

export class SettingsAppearanceFontsPageElement extends PolymerElement {
  static get is() {
    return 'settings-appearance-fonts-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      fontOptions_: Object,

      /** Common font sizes. */
      fontSizeRange_: {
        readOnly: true,
        type: Array,
        value: ticksWithLabels(FONT_SIZE_RANGE),
      },

      /** Reasonable, minimum font sizes. */
      minimumFontSizeRange_: {
        readOnly: true,
        type: Array,
        value: ticksWithLabels(MINIMUM_FONT_SIZE_RANGE),
      },

      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }

  static get observers() {
    return [
      'onMinimumSizeChange_(prefs.webkit.webprefs.minimum_font_size.value)',
    ];
  }

  prefs: Object;
  private fontOptions_: DropdownMenuOptionList;
  private fontSizeRange_: SliderTick[];
  private minimumFontSizeRange_: SliderTick[];
  private browserProxy_: FontsBrowserProxy =
      FontsBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

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
  private computeMinimumFontSize_(): number {
    const prefValue = this.get('prefs.webkit.webprefs.minimum_font_size.value');
    return prefValue || MINIMUM_FONT_SIZE_RANGE[0];
  }

  private onMinimumSizeChange_() {
    this.$.minimumSizeFontPreview.hidden = this.computeMinimumFontSize_() <= 0;
  }

  private fontFamilyValueForFixed_(prefValue: string) {
    // <if expr="is_macosx">
    // Osaka font family, which is bundled with macOS, contains a proportional
    // and a fixed-width fonts. The CSS `font-family` property distinguishes
    // them by assuming 'Osaka' for the proportional font and 'Osaka-Mono' for
    // the fixed-width font. See crbug.com/40535332.
    if (prefValue === 'Osaka') {
      return 'Osaka-Mono';
    }
    // </if>
    return prefValue;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-appearance-fonts-page': SettingsAppearanceFontsPageElement;
  }
}

customElements.define(
    SettingsAppearanceFontsPageElement.is, SettingsAppearanceFontsPageElement);
