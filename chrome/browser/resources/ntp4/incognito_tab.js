// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

window.addEventListener('load', function() {
  let cookieSettingsUrl;

  addWebUIListener('theme-changed', themeData => {
    document.documentElement.setAttribute(
        'hascustombackground', themeData.hasCustomBackground);
    $('incognitothemecss').href =
        'chrome://theme/css/incognito_tab_theme.css?' + Date.now();
  });
  chrome.send('observeThemeChanges');

  addWebUIListener('cookie-controls-changed', dict => {
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
