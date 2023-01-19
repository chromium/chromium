// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {WebAppInternalsHandler} from './web_app_internals.mojom-webui.js';

const webAppInternalsHandler = WebAppInternalsHandler.getRemote();

const debugInfoAsJsonString: Promise<string> =
    webAppInternalsHandler.getDebugInfoAsJsonString().then(
        response => response.result);

getRequiredElement('copy-button').addEventListener('click', async () => {
  navigator.clipboard.writeText(await debugInfoAsJsonString);
});

getRequiredElement('download-button').addEventListener('click', async () => {
  const url = URL.createObjectURL(new Blob([await debugInfoAsJsonString], {
    type: 'application/json',
  }));

  const a = document.createElement('a');
  a.href = url;
  a.download = 'web_app_internals.json';
  a.click();

  // Downloading succeeds even if the URL was revoked during downloading. See
  // the spec for details (https://w3c.github.io/FileAPI/#dfn-revokeObjectURL):
  //
  // "Note: ... Requests that were started before the url was revoked should
  // still succeed."
  URL.revokeObjectURL(url);
});

document.addEventListener('DOMContentLoaded', async () => {
  getRequiredElement('json').innerText = await debugInfoAsJsonString;
});
