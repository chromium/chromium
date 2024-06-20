// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandler} from './mall_ui.mojom-webui.js';

const pageHandler = PageHandler.getRemote();

async function embedMall() {
  const mallUrl = (await pageHandler.getMallEmbedUrl()).url;

  const mallFrame = document.createElement('iframe');
  mallFrame.src = mallUrl.url;
  mallFrame.setAttribute('hidden', 'true');

  mallFrame.addEventListener('load', () => {
    mallFrame.removeAttribute('hidden');
    const spinner = document.getElementById('spinner');
    if (spinner) {
      spinner.setAttribute('hidden', 'true');
    }
  });

  document.body.appendChild(mallFrame);
}

embedMall();
