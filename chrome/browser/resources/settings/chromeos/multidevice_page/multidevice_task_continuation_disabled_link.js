// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-task-continuation-disabled-link'
 * creates a localized string with accessibility labels for the Phone Hub Task
 * continuation feature when the prerequisite Chrome Sync setting is not
 * enabled.
 *
 * The localized string is treated as a special case for accessibility
 * labelling since it contains two links, one to the Chrome Sync dependency
 * and the other to a Learn More page for Phone Hub.
 */
Polymer({
  is: 'settings-multidevice-task-continuation-disabled-link',

  behaviors: [I18nBehavior],

  /** @override */
  attached() {
    const chromeSyncLink = this.$$('#chromeSyncLink');
    if (chromeSyncLink) {
      chromeSyncLink.addEventListener(
          'click', this.onChromeSyncLinkClick_.bind(this));
    }
  },

  /**
   * @return {string} Localized summary of Task Continuation when Chrome Sync is
   *     turned off, formatted with correct aria-labels and click events.
   * @private
   */
  getAriaLabelledContent_() {
    const tempEl = document.createElement('div');
    tempEl.innerHTML = this.i18nAdvanced(
        'multidevicePhoneHubTaskContinuationDisabledSummary', {attrs: ['id']});

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
        'aria-label',
        this.i18n('multidevicePhoneHubTaskContinuationSyncLabel'));
    learnMoreLink.setAttribute(
        'aria-label', this.i18n('multidevicePhoneHubLearnMoreLabel'));
    chromeSyncLink.href = '#';

    return tempEl.innerHTML;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onChromeSyncLinkClick_(event) {
    event.preventDefault();
    if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      window.open('chrome://settings/syncSetup/advanced');
      this.fire('opened-browser-advanced-sync-settings');
    } else {
      settings.Router.getInstance().navigateTo(settings.routes.SYNC_ADVANCED);
    }
  },
});
