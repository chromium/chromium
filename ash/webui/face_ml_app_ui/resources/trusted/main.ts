// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Example of fill the content.
(async () => {
  const content = document.querySelector<HTMLElement>('#app-top-bar')!;
  content.textContent = 'Welcome to the Face ML app!';
})();
