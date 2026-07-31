// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevInstallLocalBundleTabElement} from './install_local_bundle_tab.js';

export function getHtml(this: IwaDevInstallLocalBundleTabElement) {
  // clang-format off
  return html`
<p>
  Click "Install" below to select a signed web bundle (.swbn) via the
  OS file picker.
</p>
`;
}
