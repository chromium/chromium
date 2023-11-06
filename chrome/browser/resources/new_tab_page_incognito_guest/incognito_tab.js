// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

window.addEventListener('load', function() {
  let cookieSettingsUrl;

  addWebUiListener('theme-changed', themeData => {
    document.documentElement.setAttribute(
        'hascustombackground', themeData.hasCustomBackground);
    $('incognitothemecss').href =
        'chrome://theme/css/incognito_tab_theme.css?' + Date.now();
  });
  chrome.send('observeThemeChanges');

  addWebUiListener('cookie-controls-changed', dict => {
    $('cookie-controls-tooltip-icon').hidden = !dict.enforced;
    $('cookie-controls-tooltip-icon').iconClass = dict.icon;
    $('cookie-controls-toggle').disabled = dict.enforced;
    $('cookie-controls-toggle').checked = dict.checked;
    cookieSettingsUrl = dict.cookieSettingsUrl;
  });
  $('cookie-controls-toggle').addEventListener('change', event => {
    chrome.send('cookieControlsToggleChanged', [event.detail]);
  });
  // Make cookie-controls-tooltip-icon respond to the enter key.
  $('cookie-controls-tooltip-icon').addEventListener('keyup', event => {
    if (event.key === 'Enter') {
      $('cookie-controls-tooltip-icon').click();
    }
  });
  $('cookie-controls-tooltip-icon').onclick = () => {
    window.location.href = cookieSettingsUrl;
  };
  chrome.send('observeCookieControlsSettingsChanges');
});
