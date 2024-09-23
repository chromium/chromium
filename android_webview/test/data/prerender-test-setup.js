// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Objects starting with the "aw" prefix are injected by AwPrerenderTest.

function createIframe(url) {
  const iframe = document.createElement('iframe');
  iframe.setAttribute('src', url);
  document.body.appendChild(iframe);
}

function wasActivated() {
  return self.performance?.getEntriesByType?.('navigation')[0]
             ?.activationStart > 0;
}

const wasPrerendered = document.prerendering || wasActivated();

if (wasActivated()) {
  awActivationFuture.activated();
} else {
  document.onprerenderingchange = () => awActivationFuture.activated();
}

// Notify the primary page that this prerendered page started.
window.localStorage.setItem('pageStarted', location.href);
