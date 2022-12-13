// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-wifi-sync-disabled-link' creates a
 * localized string with accessibility labels for the Wifi Sync feature when
 * the prerequisite Chrome Sync setting is not enabled.
 *
 * The localized string is treated as a special case for accessibility
 * labelling since it contains two links, one to the Chrome Sync dependency
 * and the other to a Learn More page for Wifi Sync.
 */

import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Router} from '../router.js';
import {routes} from '../os_route.js';

import {MultiDeviceFeatureBehavior, MultiDeviceFeatureBehaviorInterface} from './multidevice_feature_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {MultiDeviceFeatureBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsMultideviceWifiSyncDisabledLinkElementBase =
    mixinBehaviors([MultiDeviceFeatureBehavior, I18nBehavior], PolymerElement);

/** @polymer */
class SettingsMultideviceWifiSyncDisabledLinkElement extends
    SettingsMultideviceWifiSyncDisabledLinkElementBase {
  static get is() {
    return 'settings-multidevice-wifi-sync-disabled-link';
  }

  static get template() {
    return html`{__html_template__}`;
  }

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
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    const chromeSyncLink = this.shadowRoot.querySelector('#chromeSyncLink');
    if (chromeSyncLink) {
      chromeSyncLink.addEventListener(
          'click', this.onChromeSyncLinkClick_.bind(this));
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onChromeSyncLinkClick_(event) {
    event.preventDefault();
    Router.getInstance().navigateTo(routes.OS_SYNC);
  }
}

customElements.define(
    SettingsMultideviceWifiSyncDisabledLinkElement.is,
    SettingsMultideviceWifiSyncDisabledLinkElement);
