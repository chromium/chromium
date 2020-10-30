// Copyright 2020 The Chromium Authors. All rights reserved.
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
Polymer({
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
    if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      settings.Router.getInstance().navigateTo(settings.routes.OS_SYNC);
    } else {
      settings.Router.getInstance().navigateTo(settings.routes.SYNC_ADVANCED);
    }
  },
});
