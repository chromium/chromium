// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface PageToolbarElement extends LegacyElementMixin, HTMLElement {
  title: string;
  isNarrow: boolean;
  onMenuTap_(): void;
}

export {PageToolbarElement};

declare global {
  interface HTMLElementTagNameMap {
    'page-toolbar': PageToolbarElement;
  }
}