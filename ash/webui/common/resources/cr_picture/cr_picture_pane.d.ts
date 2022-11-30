// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface CrPicturePaneElement extends HTMLElement {}

export {CrPicturePaneElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-picture-pane': CrPicturePaneElement;
  }
}
