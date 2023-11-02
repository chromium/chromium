// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This snippet of code must run synchronously (not as a module), before the
// <iframe> element is encountered in the DOM. This is to ensure there is a
// receiver for the very first message that is received from the <iframe>. That
// message is the one that indicates that the <iframe> is ready to receive its
// own messages over window.postMessage().
window.firstMessageReceived = new Promise(resolve => {
  window.addEventListener('message', resolve, {once: true});
});
