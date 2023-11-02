// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_elements/button/button.js';

customElements.whenDefined('cros-button').then(() => {
  document.documentElement.classList.remove('loading');
});
