// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/button/button.js';

customElements.whenDefined('cros-button').then(() => {
  document.documentElement.classList.remove('loading');
});
