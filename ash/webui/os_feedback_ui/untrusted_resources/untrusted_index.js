// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {startColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';

import {HelpContentElement} from './help_content.js';

/**
 * The host of trusted parent page.
 * @type {string}
 */
const OS_FEEDBACK_TRUSTED_ORIGIN = 'chrome://os-feedback';

function initialize() {
  /**
   * The help-content custom element.
   * @type {!HelpContentElement}
   */
  const helpContent = document.querySelector('help-content');

  window.addEventListener('message', event => {
    if (event.origin !== OS_FEEDBACK_TRUSTED_ORIGIN) {
      console.error('Unknown origin: ' + event.origin);
      return;
    }
    // After receiving search result sent from parent page, display them.
    helpContent.searchResult = event.data;

    // Post a message to parent to make testing easier.
    window.parent.postMessage(
        {
          id: 'help-content-received-for-testing',
          count: event.data.contentList.length,
        },
        OS_FEEDBACK_TRUSTED_ORIGIN);
  });

  if (loadTimeData.getBoolean('isJellyEnabledForOsFeedback')) {
    // TODO(b/276493287): After the Jelly experiment is launched, replace
    // `cros_styles.css` with `theme/colors.css` directly in `index.html`.
    // Also add `theme/typography.css` to `index.html`.
    document.querySelector('link[href*=\'cros_styles.css\']')
        ?.setAttribute('href', '//theme/colors.css?sets=legacy,sys');
    const typographyLink = document.createElement('link');
    typographyLink.href = '//theme/typography.css';
    typographyLink.rel = 'stylesheet';
    document.head.appendChild(typographyLink);
    document.body.classList.add('jelly-enabled');
    startColorChangeUpdater();
    // Post a message to parent to make testing `startColorChangeUpdater()`
    // called from untrusted ui easier.
    window.parent.postMessage(
        {
          id: 'color-change-updater-started-for-testing',
        },
        OS_FEEDBACK_TRUSTED_ORIGIN);
  }
}

document.addEventListener('DOMContentLoaded', initialize);
