// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandler} from './mall_ui.mojom-webui.js';

const pageHandler = PageHandler.getRemote();

const spinner = document.getElementById('spinner');
const offline = document.getElementById('offline');

// Create mall iframe.
const mallFrame = document.createElement('iframe');
mallFrame.setAttribute('hidden', 'true');
document.body.appendChild(mallFrame);

async function loadMall() {
  mallFrame.src =
      (await pageHandler.getMallEmbedUrl(window.location.pathname)).url.url;
}

// Load now and attempt every 30s if network fails.
loadMall();
const reloadInterval = setInterval(loadMall, 30000);

// Show the offline page if loading takes more than 15s.
const offlineTimeout = setTimeout(() => {
  spinner?.setAttribute('hidden', 'true');
  offline?.removeAttribute('hidden');
}, 15000);

// When we receive a postMessage from the iframe, it has loaded ok.
window.addEventListener('message', () => {
  clearTimeout(offlineTimeout);
  clearInterval(reloadInterval);
  spinner?.setAttribute('hidden', 'true');
  offline?.setAttribute('hidden', 'true');
  mallFrame.removeAttribute('hidden');
}, {once: true});
