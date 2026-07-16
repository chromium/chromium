// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-captions-page' is a component for showing captions
 * settings on chrome://settings/captions.
 */

import '//resources/cr_elements/cr_shared_style.css.js';
import '../controls/settings_dropdown_menu.js';
import '../controls/settings_slider.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import './live_caption.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {FontsData} from '/shared/settings/appearance_page/fonts_browser_proxy.js';
import {FontsBrowserProxyImpl} from '/shared/settings/appearance_page/fonts_browser_proxy.js';
import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';

import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './captions_page.html.js';

const SettingsCaptionsElementBase =
    SettingsViewMixin(PrefServiceObserverMixin(PolymerElement));

export class SettingsCaptionsElement extends SettingsCaptionsElementBase {
  static get is() {
    return 'settings-captions-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of options for the background opacity drop-down menu.
       */
      backgroundOpacityOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 100,  // Default
              name: loadTimeData.getString('captionsOpacityOpaque'),
            },
            {
              value: 50,
              name: loadTimeData.getString('captionsOpacitySemiTransparent'),
            },
            {
              value: 0,
              name: loadTimeData.getString('captionsOpacityTransparent'),
            },
          ];
        },
      },

      /**
       * List of options for the color drop-down menu.
       */
      colorOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {value: '', name: loadTimeData.getString('captionsDefaultSetting')},
            {
              value: '0,0,0',
              name: loadTimeData.getString('captionsColorBlack'),
            },
            {
              value: '255,255,255',
              name: loadTimeData.getString('captionsColorWhite'),
            },
            {
              value: '255,0,0',
              name: loadTimeData.getString('captionsColorRed'),
            },
            {
              value: '0,255,0',
              name: loadTimeData.getString('captionsColorGreen'),
            },
            {
              value: '0,0,255',
              name: loadTimeData.getString('captionsColorBlue'),
            },
            {
              value: '255,255,0',
              name: loadTimeData.getString('captionsColorYellow'),
            },
            {
              value: '0,255,255',
              name: loadTimeData.getString('captionsColorCyan'),
            },
            {
              value: '255,0,255',
              name: loadTimeData.getString('captionsColorMagenta'),
            },
          ];
        },
      },

      /**
       * List of fonts populated by the fonts browser proxy.
       */
      textFontOptions_: Object,

      /**
       * List of options for the text opacity drop-down menu.
       */
      textOpacityOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 100,  // Default
              name: loadTimeData.getString('captionsOpacityOpaque'),
            },
            {
              value: 50,
              name: loadTimeData.getString('captionsOpacitySemiTransparent'),
            },
            {
              value: 10,
              name: loadTimeData.getString('captionsOpacityTransparent'),
            },
          ];
        },
      },

      /**
       * List of options for the text shadow drop-down menu.
       *
       * Other clients are relying on these values to determine text shadow type
       * from preference. Please update the following files if any of these
       * values are changed:
       * https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/ash/settings/os_a11y_page/captions_subpage.ts;l=142-170;drc=0918c7f73782a9575396f0c6b80a722b5a3d255a
       * https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/arc/intent_helper/arc_settings_service.cc;l=86-94;drc=a782e6ac5124014b8473c9e7e445d799624b532c
       */
      textShadowOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {value: '', name: loadTimeData.getString('captionsTextShadowNone')},
            {
              value: '-2px -2px 4px rgba(0, 0, 0, 0.5)',
              name: loadTimeData.getString('captionsTextShadowRaised'),
            },
            {
              value: '2px 2px 4px rgba(0, 0, 0, 0.5)',
              name: loadTimeData.getString('captionsTextShadowDepressed'),
            },
            {
              value: '-1px 0px 0px black, ' +
                  '0px -1px 0px black, 1px 0px 0px black, 0px  1px 0px black',
              name: loadTimeData.getString('captionsTextShadowUniform'),
            },
            {
              value: '0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black',
              name: loadTimeData.getString('captionsTextShadowDropShadow'),
            },
          ];
        },
      },

      /**
       * List of options for the text size drop-down menu.
       */
      textSizeOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {value: '25%', name: loadTimeData.getString('verySmall')},
            {value: '50%', name: loadTimeData.getString('small')},
            {
              value: '',
              name: loadTimeData.getString('medium'),
            },  // Default = 100%
            {value: '150%', name: loadTimeData.getString('large')},
            {value: '200%', name: loadTimeData.getString('veryLarge')},
          ];
        },
      },

      enableLiveCaption_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveCaption');
        },
      },

      backgroundColorPref_: Object,
      backgroundOpacityPref_: Object,
      textColorPref_: Object,
      textFontPref_: Object,
      textOpacityPref_: Object,
      textShadowPref_: Object,
      textSizePref_: Object,
    };
  }

  declare private readonly backgroundOpacityOptions_: DropdownMenuOptionList;
  declare private readonly colorOptions_: DropdownMenuOptionList;
  declare private textFontOptions_: DropdownMenuOptionList;
  declare private readonly textOpacityOptions_: DropdownMenuOptionList;
  declare private readonly textShadowOptions_: DropdownMenuOptionList;
  declare private readonly textSizeOptions_: DropdownMenuOptionList;
  declare private enableLiveCaption_: boolean;

  declare private backgroundColorPref_:
      chrome.settingsPrivate.PrefObject<string>|undefined;
  declare private backgroundOpacityPref_:
      chrome.settingsPrivate.PrefObject<number>|undefined;
  declare private textColorPref_: chrome.settingsPrivate.PrefObject<string>|
      undefined;
  declare private textFontPref_: chrome.settingsPrivate.PrefObject<string>|
      undefined;
  declare private textOpacityPref_: chrome.settingsPrivate.PrefObject<number>|
      undefined;
  declare private textShadowPref_: chrome.settingsPrivate.PrefObject<string>|
      undefined;
  declare private textSizePref_: chrome.settingsPrivate.PrefObject<string>|
      undefined;

  override connectedCallback() {
    super.connectedCallback();
    this.mirrorPrefs({
      'accessibility.captions.background_color': 'backgroundColorPref_',
      'accessibility.captions.background_opacity': 'backgroundOpacityPref_',
      'accessibility.captions.text_color': 'textColorPref_',
      'accessibility.captions.text_font': 'textFontPref_',
      'accessibility.captions.text_opacity': 'textOpacityPref_',
      'accessibility.captions.text_shadow': 'textShadowPref_',
      'accessibility.captions.text_size': 'textSizePref_',
    });
  }

  override ready() {
    super.ready();
    FontsBrowserProxyImpl.getInstance().fetchFontsData().then(
        (response: FontsData) => this.setFontsData_(response));
  }

  /**
   * @return the Live Caption toggle element.
   */
  getLiveCaptionToggle(): SettingsToggleButtonElement|null {
    const liveCaptionSection =
        this.shadowRoot!.querySelector('settings-live-caption');
    return liveCaptionSection ? liveCaptionSection.getLiveCaptionToggle() :
                                null;
  }

  /**
   * @param response A list of fonts.
   */
  private setFontsData_(response: FontsData) {
    const fontMenuOptions =
        [{value: '', name: loadTimeData.getString('captionsDefaultSetting')}];
    for (const fontData of response.fontList) {
      fontMenuOptions.push({value: fontData[0], name: fontData[1]});
    }
    this.textFontOptions_ = fontMenuOptions;
  }

  /**
   * @return the font family as a CSS property value.
   */
  private getFontFamily_(): string {
    const fontFamily = this.textFontPref_?.value;

    // Return the preference value or the default font family for
    // video::-webkit-media-text-track-container defined in mediaControls.css.
    return fontFamily || 'sans-serif';
  }

  /**
   * @return the background color as a RGBA string.
   */
  private computeBackgroundColor_(): string {
    const backgroundColor = this.formatRgbaString_(
        this.backgroundColorPref_?.value, this.backgroundOpacityPref_?.value);

    // Return the preference value or the default background color for
    // video::cue defined in mediaControls.css.
    return backgroundColor || 'rgba(0, 0, 0, 0.8)';
  }

  /**
   * @return the text color as a RGBA string.
   */
  private computeTextColor_(): string {
    const textColor = this.formatRgbaString_(
        this.textColorPref_?.value, this.textOpacityPref_?.value);

    // Return the preference value or the default text color for
    // video::-webkit-media-text-track-container defined in mediaControls.css.
    return textColor || 'rgba(255, 255, 255, 1)';
  }

  /**
   * Formats the color as an RGBA string.
   * @param color The RGB values as a comma-separated string.
   * @param opacity The opacity value as a percentage.
   * @return The formatted RGBA string.
   */
  private formatRgbaString_(color?: string, opacity?: number): string {
    if (!color || opacity === undefined) {
      return '';
    }

    return 'rgba(' + color + ',' + opacity / 100.0 + ')';
  }

  /**
   * @param size The font size of the captions text as a percentage.
   * @return The padding around the captions text as a percentage.
   */
  private computePadding_(size: string): string {
    if (size === '') {
      return '1%';
    }

    return `${+ size.slice(0, -1) / 100}%`;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-captions-page': SettingsCaptionsElement;
  }
}

customElements.define(SettingsCaptionsElement.is, SettingsCaptionsElement);
