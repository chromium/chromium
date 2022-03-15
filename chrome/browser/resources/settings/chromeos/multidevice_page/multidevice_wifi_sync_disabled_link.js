// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../settings_shared_css.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Router} from '../../router.js';
import {routes} from '../os_route.m.js';

import {MultiDeviceFeatureBehavior} from './multidevice_feature_behavior.js';

/**
 * @fileoverview 'settings-multidevice-wifi-sync-disabled-link' creates a
 * localized string with accessibility labels for the Wifi Sync feature when
 * the prerequisite Chrome Sync setting is not enabled.
 *
 * The localized string is treated as a special case for accessibility
 * labelling since it contains two links, one to the Chrome Sync dependency
 * and the other to a Learn More page for Wifi Sync.
 */
Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-multidevice-wifi-sync-disabled-link',

  behaviors: [
    MultiDeviceFeatureBehavior,
  ],

  getAriaLabelledContent_() {
    const tempEl = document.createElement('div');
    tempEl.innerHTML = this.i18nAdvanced(
        'multideviceEnableWifiSyncV1ItemSummary', {attrs: ['id']});

    tempEl.childNodes.forEach((node, index) => {
      // Text nodes should be aria-hidden
      if (node.nodeType === Node.TEXT_NODE) {
        const spanNode = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `id${index}`;
        spanNode.setAttribute('aria-hidden', true);
        node.replaceWith(spanNode);
        return;
      }
    });

    const chromeSyncLink = tempEl.querySelector('#chromeSyncLink');
    const learnMoreLink = tempEl.querySelector('#learnMoreLink');

    chromeSyncLink.setAttribute(
        'aria-label', this.i18n('multideviceWifiSyncChromeSyncLabel'));
    learnMoreLink.setAttribute(
        'aria-label', this.i18n('multideviceWifiSyncLearnMoreLabel'));
    chromeSyncLink.href = '#';

    return tempEl.innerHTML;
  },

  /** @override */
  attached() {
    const chromeSyncLink = this.$$('#chromeSyncLink');
    if (chromeSyncLink) {
      chromeSyncLink.addEventListener(
          'click', this.onChromeSyncLinkClick_.bind(this));
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onChromeSyncLinkClick_(event) {
    event.preventDefault();
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled')) {
      // If syncSettingsCategorization is enabled, then WiFi sync is controlled
      // by the OS sync settings, not the browser sync settings.
      Router.getInstance().navigateTo(routes.OS_SYNC);
    } else {
      Router.getInstance().navigateTo(routes.SYNC_ADVANCED);
    }
  },
});
