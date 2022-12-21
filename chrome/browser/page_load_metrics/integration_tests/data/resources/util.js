// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns a promise that is resolved when the specified number of animation
// frames has occurred.
async function waitUntilAfterNextLayout() {
  let frameCount = 2;
    return new Promise(resolve => {
        const handleFrame = () => {
        if (--frameCount <= 0)
            resolve();
        else
            requestAnimationFrame(handleFrame);
        };
        requestAnimationFrame(handleFrame);
    });
};

async function observeUntilNumEntries(n, opts) {
  return new Promise((resolve) => {
    const entries = [];
    const observer = new PerformanceObserver((list) => {
      for (const entry of list.getEntries()) {
        entries.push(entry);
      }

      if (entries.length >= n) {
        observer.disconnect();
        resolve(entries);
      }
    });
    observer.observe(opts);
  });
}

async function waitForNumLayoutShiftEntries(num) {
  let entries = await observeUntilNumEntries(num, {
    type: 'layout-shift',
    buffered: true,
  });

  return entries.map((entry) => ({
    startTime: entry.startTime,
    score: entry.value,
    hadRecentInput: entry.hadRecentInput,
  }));
}
