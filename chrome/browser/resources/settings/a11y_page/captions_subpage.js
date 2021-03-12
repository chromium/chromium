// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-captions' is a component for showing captions
 * settings subpage (chrome://settings/captions).
 */

import '//resources/cr_elements/shared_style_css.m.js';
import '../controls/settings_slider.js';
import '../settings_shared_css.js';
import './live_caption_section.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FontsBrowserProxy, FontsBrowserProxyImpl, FontsData} from '../appearance_page/fonts_browser_proxy.js';
import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {loadTimeData} from '../i18n_setup.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';


Polymer({
  is: 'settings-captions',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    PrefsBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * List of options for the background opacity drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    backgroundOpacityOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {
            value: 100,  // Default
            name: loadTimeData.getString('captionsOpacityOpaque')
          },
          {
            value: 50,
            name: loadTimeData.getString('captionsOpacitySemiTransparent')
          },
          {
            value: 0,
            name: loadTimeData.getString('captionsOpacityTransparent')
          },
        ];
      },
    },

    /**
     * List of options for the color drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    colorOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {value: '', name: loadTimeData.getString('captionsDefaultSetting')},
          {value: '0,0,0', name: loadTimeData.getString('captionsColorBlack')},
          {
            value: '255,255,255',
            name: loadTimeData.getString('captionsColorWhite')
          },
          {value: '255,0,0', name: loadTimeData.getString('captionsColorRed')},
          {
            value: '0,255,0',
            name: loadTimeData.getString('captionsColorGreen')
          },
          {value: '0,0,255', name: loadTimeData.getString('captionsColorBlue')},
          {
            value: '255,255,0',
            name: loadTimeData.getString('captionsColorYellow')
          },
          {
            value: '0,255,255',
            name: loadTimeData.getString('captionsColorCyan')
          },
          {
            value: '255,0,255',
            name: loadTimeData.getString('captionsColorMagenta')
          },
        ];
      },
    },

    /**
     * List of fonts populated by the fonts browser proxy.
     * @private {!DropdownMenuOptionList}
     */
    textFontOptions_: Object,

    /**
     * List of options for the text opacity drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    textOpacityOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {
            value: 100,  // Default
            name: loadTimeData.getString('captionsOpacityOpaque')
          },
          {
            value: 50,
            name: loadTimeData.getString('captionsOpacitySemiTransparent')
          },
          {
            value: 10,
            name: loadTimeData.getString('captionsOpacityTransparent')
          },
        ];
      },
    },

    /**
     * List of options for the text shadow drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    textShadowOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {value: '', name: loadTimeData.getString('captionsTextShadowNone')},
          {
            value: '-2px -2px 4px rgba(0, 0, 0, 0.5)',
            name: loadTimeData.getString('captionsTextShadowRaised')
          },
          {
            value: '2px 2px 4px rgba(0, 0, 0, 0.5)',
            name: loadTimeData.getString('captionsTextShadowDepressed')
          },
          {
            value: '-1px 0px 0px black, ' +
                '0px -1px 0px black, 1px 0px 0px black, 0px  1px 0px black',
            name: loadTimeData.getString('captionsTextShadowUniform')
          },
          {
            value: '0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black',
            name: loadTimeData.getString('captionsTextShadowDropShadow')
          },
        ];
      },
    },

    /**
     * List of options for the text size drop-down menu.
     * @type {!DropdownMenuOptionList}
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
            name: loadTimeData.getString('medium')
          },  // Default = 100%
          {value: '150%', name: loadTimeData.getString('large')},
          {value: '200%', name: loadTimeData.getString('veryLarge')},
        ];
      },
    },

    /** @private */
    enableLiveCaption_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableLiveCaption');
      },
    },
  },

  /** @private {?FontsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = FontsBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.browserProxy_.fetchFontsData().then(this.setFontsData_.bind(this));
  },

  /**
   * Returns the Live Caption toggle element.
   * @return {?CrToggleElement}
   */
  getLiveCaptionToggle() {
    const liveCaptionSection = this.$$('settings-live-caption');
    return liveCaptionSection ? liveCaptionSection.getLiveCaptionToggle() :
                                null;
  },

  /**
   * @param {!FontsData} response A list of fonts.
   * @private
   */
  setFontsData_(response) {
    const fontMenuOptions =
        [{value: '', name: loadTimeData.getString('captionsDefaultSetting')}];
    for (const fontData of response.fontList) {
      fontMenuOptions.push({value: fontData[0], name: fontData[1]});
    }
    this.textFontOptions_ = fontMenuOptions;
  },

  /**
   * Get the font family as a CSS property value.
   * @return {string}
   * @private
   */
  getFontFamily_() {
    const fontFamily = this.getPref('accessibility.captions.text_font').value;

    // Return the preference value or the default font family for
    // video::-webkit-media-text-track-container defined in mediaControls.css.
    return /** @type {string} */ (fontFamily || 'sans-serif');
  },

  /**
   * Get the background color as a RGBA string.
   * @return {string}
   * @private
   */
  computeBackgroundColor_() {
    const backgroundColor = this.formatRGAString_(
        'accessibility.captions.background_color',
        'accessibility.captions.background_opacity');

    // Return the preference value or the default background color for
    // video::cue defined in mediaControls.css.
    return backgroundColor || 'rgba(0, 0, 0, 0.8)';
  },

  /**
   * Get the text color as a RGBA string.
   * @return {string}
   * @private
   */
  computeTextColor_() {
    const textColor = this.formatRGAString_(
        'accessibility.captions.text_color',
        'accessibility.captions.text_opacity');

    // Return the preference value or the default text color for
    // video::-webkit-media-text-track-container defined in mediaControls.css.
    return textColor || 'rgba(255, 255, 255, 1)';
  },

  /**
   * Formats the color as an RGBA string.
   * @param {string} colorPreference The name of the preference containing the
   * RGB values as a comma-separated string.
   * @param {string} opacityPreference The name of the preference containing
   * the opacity value as a percentage.
   * @return {string} The formatted RGBA string.
   * @private
   */
  formatRGAString_(colorPreference, opacityPreference) {
    const color = this.getPref(colorPreference).value;

    if (!color) {
      return '';
    }

    return 'rgba(' + color + ',' +
        parseInt(this.getPref(opacityPreference).value, 10) / 100.0 + ')';
  },

  /**
   * @param {string} size The font size of the captions text as a percentage.
   * @return {string} The padding around the captions text as a percentage.
   * @private
   */
  computePadding_(size) {
    if (size === '') {
      return '1%';
    }

    return `${+ size.slice(0, -1) / 100}%`;
  },
});
