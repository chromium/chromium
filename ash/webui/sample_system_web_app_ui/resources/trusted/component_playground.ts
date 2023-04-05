// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Examples of importing lit, cros components, and @material elements.
import {Button} from 'chrome://resources/cros_components/button/button.js';
import {MdTextButton} from 'chrome://resources/mwc/@material/web/button/text-button.js';
import {html} from 'chrome://resources/mwc/lit/index.js';

// Log them to avoid unused import errors.
console.info(html);
console.info(Button);
console.info(MdTextButton);

customElements.whenDefined('cros-button').then(() => {
  document.documentElement.classList.remove('loading');
});
