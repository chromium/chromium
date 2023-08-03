// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './multidevice_setup_shared.css.js';
import './ui_page.js';
import '//resources/ash/common/cr.m.js';
import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl} from './multidevice_setup_browser_proxy.js';
import {getTemplate} from './setup_succeeded_page.html.js';
import {UiPageContainerBehavior} from './ui_page_container_behavior.js';

/**
 * TODO(b/279667779): Remove when Jelly is fully launched.
 * @type {string}
 */
const SRC_SET_URL_1_LIGHT =
    'chrome://resources/ash/common/multidevice_setup/all_set_1x_light.svg';

/**
 * TODO(b/279667779): Remove when Jelly is fully launched.
 * @type {string}
 */
const SRC_SET_URL_2_LIGHT =
    'chrome://resources/ash/common/multidevice_setup/all_set_2x_light.svg';

/**
 * TODO(b/279667779): Remove when Jelly is fully launched.
 * @type {string}
 */
const SRC_SET_URL_1_DARK =
    'chrome://resources/ash/common/multidevice_setup/all_set_1x_dark.svg';

/**
 * TODO(b/279667779): Remove when Jelly is fully launched.
 * @type {string}
 */
const SRC_SET_URL_2_DARK =
    'chrome://resources/ash/common/multidevice_setup/all_set_2x_dark.svg';

Polymer({
  _template: getTemplate(),
  is: 'setup-succeeded-page',

  properties: {
    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'done',
    },

    /**
     * Whether the multidevice success page is being rendered in dark mode.
     * TODO(b/279667779): Remove when Jelly is fully launched.
     * @private {boolean}
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the multidevice setup page is being rendered with dynamic colors.
     * @private {boolean}
     */
    isJellyEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('isJellyEnabled') &&
            loadTimeData.getBoolean('isJellyEnabled');
      },
    },
  },

  behaviors: [UiPageContainerBehavior],

  /** @private {?BrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = BrowserProxyImpl.getInstance();
  },

  /** @private */
  openSettings_() {
    this.browserProxy_.openMultiDeviceSettings();
  },

  /** @private */
  onSettingsLinkClicked_() {
    this.openSettings_();
    this.fire('setup-exited');
  },

  /** @private */
  getMessageHtml_() {
    return this.i18nAdvanced('setupSucceededPageMessage', {attrs: ['id']});
  },

  /** @override */
  ready() {
    const linkElement = this.$$('#settings-link');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', () => this.onSettingsLinkClicked_());
  },

  /**
   * Returns source set for images based on if the page is rendered in dark
   * mode.
   * TODO(b/279667779): Remove when Jelly is fully launched.
   * @return {string}
   * @private
   */
  getImageSrcSet_() {
    return this.isDarkModeActive_ ?
        SRC_SET_URL_1_DARK + ' 1x, ' + SRC_SET_URL_2_DARK + ' 2x' :
        SRC_SET_URL_1_LIGHT + ' 1x, ' + SRC_SET_URL_2_LIGHT + ' 2x';
  },
});
