// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tries = 10;

// Google Docs isn't compatible with Select-to-speak by default because
// in order to provide screen reader support, most of the rendered
// document has aria-hidden set on it, which has the side effect of
// hiding it from Select-to-speak too. Fix it by changing aria-hidden
// to false. Try multiple times in case the page isn't fully loaded when
// the content script runs.
function RemoveAriaHiddenFromGoogleDocsContent() {
  var element = document.querySelector('.kix-zoomdocumentplugin-outer');
  if (element) {
    element.setAttribute('aria-hidden', 'false');
  } else {
    tries--;
    if (tries > 0) {
      window.setTimeout(RemoveAriaHiddenFromGoogleDocsContent, 1000);
    }
  }
}

RemoveAriaHiddenFromGoogleDocsContent();
