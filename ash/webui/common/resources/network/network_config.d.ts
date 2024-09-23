// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from '//resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';
interface NetworkConfigElement extends LegacyElementMixin, HTMLElement {
  enableConnect: boolean;
  init(): void;
  connect(): void;
  save(): void;
}
export {NetworkConfigElement};
