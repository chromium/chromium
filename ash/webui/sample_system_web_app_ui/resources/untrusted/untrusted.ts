// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {PARENT_PAGE_ORIGIN, parentPage} from './untrusted_page_interface.js';

const header = document.querySelector<HTMLTitleElement>('#untrusted-title')!;
header.textContent = 'Sample System Web App Untrusted Page';

// For testing purposes: notify the parent window the iframe has been embedded
// successfully.
window.addEventListener('message', event => {
  if (event.origin.startsWith(PARENT_PAGE_ORIGIN)) {
    window.parent.postMessage(
        {id: 'post-message', success: true}, PARENT_PAGE_ORIGIN);
  }
});

// For testing, perform Mojo method call if the test asks for it.
window.addEventListener('message', async event => {
  if (event.data.id === 'test-mojo-method-call') {
    const {resp} = await parentPage.doSomethingForChild('Say hello');
    window.parent.postMessage(
        {
          id: 'mojo-method-call-resp',
          resp,
        },
        PARENT_PAGE_ORIGIN);
  }
});

// Ask the parent page to do something, and retrieve the result.
parentPage.doSomethingForChild('Say hello').then(result => {
  document.querySelector<HTMLParagraphElement>('#parent-resp')!.innerText =
      result.resp;
});
