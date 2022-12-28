// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {string} Type of the source file to preview.
 */
let type;

/**
 * @type {string} The content URL of the source file to preview.
 */
let contentUrl = '';

/**
 * <style> element for (non-PDF) browsable content.
 */
const style = document.createElement('style');

/**
 * Source for styles to apply to (non-PDF) browsable content.
 */
const styleSource = 'files_text_content.css';

/**
 * Apply custom CSS to iframe content.
 */
function applyTextCss() {
  if (!type) {
    return;
  }

  const iframe = document.querySelector('#content');
  const styleParent = iframe.contentDocument.head;

  // Don't override the Chrome PDF viewer's CSS: crbug.com/1001034.
  if (type === 'PDF') {
    if (style.parentNode === styleParent) {
      styleParent.removeChild(style);
    }
    return;
  }

  if (style.parentNode !== styleParent) {
    if (style.innerText) {
      styleParent.appendChild(style);
      return;
    }
    fetch(styleSource).then(response => response.text()).then(data => {
      style.innerText = data;
      styleParent.appendChild(style);
    });
  }
}

document.addEventListener('DOMContentLoaded', event => {
  const contentsIframe = document.querySelector('#content');

  // Update the contentsIframe's CSS styles each time a new source is loaded.
  contentsIframe.onload = () => applyTextCss();
});

window.addEventListener('message', event => {
  if (event.origin !== FILES_APP_SWA_ORIGIN) {
    console.error(`invalid origin: ${event.origin}`);
    return;
  }

  // Release Object URLs generated with URL.createObjectURL.
  URL.revokeObjectURL(contentUrl);
  contentUrl = '';

  const {browsable, subtype, sourceContent} = event.data;
  switch (sourceContent.dataType) {
    case 'url':
      contentUrl = sourceContent.data;
      break;
    case 'blob':
      contentUrl = URL.createObjectURL(sourceContent.data);
      break;
  }

  let sourceUrl = contentUrl;
  if (sourceUrl && browsable && subtype === 'PDF') {
    sourceUrl += '#view=FitH';
  }

  type = subtype;
  if (!sourceUrl) {
    sourceUrl = 'about:blank';
    type = '';
  }

  const contentsIframe = document.querySelector('#content');
  console.log('Setting iframe.src to:', sourceUrl);
  contentsIframe.src = sourceUrl;
});
