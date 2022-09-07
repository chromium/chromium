// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
}

document.addEventListener('DOMContentLoaded', initialize);
