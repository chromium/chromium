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

const getLcp = async (resource) => {
  return await new Promise(resolve => {
    const observer = new PerformanceObserver((list) => {
      const entries = list.getEntries().filter(e => e.url.includes(resource));
      if (entries.length > 0) {
        resolve(entries[entries.length - 1].startTime)
      }
    });
    observer.observe({ type: "largest-contentful-paint", buffered: true });
  })
}


const getRequestStart = (name) =>
  getResourceTimingEntry(name).requestStart;

const getResponseEnd = (name) =>
  getResourceTimingEntry(name).responseEnd;

const getStartTime = (name) =>
  getResourceTimingEntry(name).startTime;


const getResourceTimingEntry = (name) => {
  return performance.getEntriesByType('resource').filter(
    e => e.name.includes(name))[0];
}

const addImageWithUrl = async (url) => {
  await new Promise(resolve => {
    const img = document.createElement('img');
    img.addEventListener('load', resolve);
    img.src = url;
    document.body.appendChild(img);
  });
}