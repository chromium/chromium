// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandler} from '/ash/webui/files_internals/mojom/files_internals.mojom-webui.js';

const pageHandler = PageHandler.getRemote();

document.addEventListener('DOMContentLoaded', async () => {
  const verboseElem = document.getElementById('smb-verbose-logging-toggle');
  verboseElem.checked =
      (await pageHandler.getSmbfsEnableVerboseLogging()).enabled;
  verboseElem.addEventListener('change', (e) => {
    pageHandler.setSmbfsEnableVerboseLogging(e.target.checked);
  });
});
