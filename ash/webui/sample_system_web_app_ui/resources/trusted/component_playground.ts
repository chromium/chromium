// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '@material/web/button/text-button.js';

customElements.whenDefined('md-text-button').then(() => {
  document.documentElement.classList.remove('loading');
});
