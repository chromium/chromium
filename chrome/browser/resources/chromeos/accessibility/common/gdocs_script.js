// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tries = 10;

function triggerDocsHtmlFallbackMode() {
  const scriptContents = `
    window['_docs_force_html_by_ext'] = "${chrome.runtime.id}";
  `;
  const script = document.createElement('script');
  script.innerHTML = scriptContents;
  document.documentElement.appendChild(script);
}

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

// Docs will soon only render its contents in canvas. As a stop gap measure,
// trigger Docs' fallback html rendering mode. This needs to run within the
// page's context, so install a script to be executed.
triggerDocsHtmlFallbackMode();

// Google Docs isn't compatible with non-screen reader accessibility services by
// default because in order to provide screen reader support, most of the
// rendered document has aria-hidden set on it, which has the side effect of
// hiding it from non-screen reader accessibility features too. Fix it by
// changing aria-hidden to false. Try multiple times in case the page isn't
// fully loaded when the content script runs.
RemoveAriaHiddenFromGoogleDocsContent();
