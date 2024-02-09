// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import '//os-feedback/help_content.js';

import {HelpContentElement} from '//os-feedback/help_content.js';
import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from '//resources/js/assert.js';

/* The host of trusted parent page. */
const OS_FEEDBACK_TRUSTED_ORIGIN = 'chrome://os-feedback';

function initialize() {
  /* The help-content custom element. */
  const helpContent =
      document.querySelector<HelpContentElement>('help-content');
  assert(!!helpContent);

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
  ColorChangeUpdater.forDocument().start();
  // Post a message to parent to make testing `ColorChangeUpdater#start()`
  // called from untrusted ui easier.
  window.parent.postMessage(
      {
        id: 'color-change-updater-started-for-testing',
      },
      OS_FEEDBACK_TRUSTED_ORIGIN);
}

document.addEventListener('DOMContentLoaded', initialize);
