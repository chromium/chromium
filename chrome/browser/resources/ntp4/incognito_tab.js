// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('load', function() {
  cr.addWebUIListener('theme-changed', themeData => {
    document.documentElement.setAttribute(
        'hascustombackground', themeData.hasCustomBackground);
    $('incognitothemecss').href =
        'chrome://theme/css/incognito_new_tab_theme.css?' + Date.now();
  });
  chrome.send('observeThemeChanges');

  cr.addWebUIListener('cookie-controls-changed', dict => {
    $('cookie-controls-tooltip-icon-wrapper').hidden = !dict.enforced;
    $('cookie-controls-toggle').disabled = dict.enforced;
    $('cookie-controls-toggle').checked = dict.checked;
  });
  $('cookie-controls-toggle').addEventListener('change', event => {
    chrome.send('cookieControlsToggleChanged', [event.detail]);
  });
  chrome.send('observeCookieControlsSettingsChanges');
});

// Handle the bookmark bar, theme, and font size change requests
// from the C++ side.
const ntp = {
  /** @param {string} attached */
  setBookmarkBarAttached: function(attached) {
    document.documentElement.setAttribute('bookmarkbarattached', attached);
  },
};
