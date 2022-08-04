// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Example of fill the content.
(async () => {
  const content = document.querySelector<HTMLElement>('#content')!;
  content.textContent = 'Hello Facial ML user!';
})();
