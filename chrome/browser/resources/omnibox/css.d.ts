// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See https://github.com/microsoft/TypeScript/issues/46135.
declare module '*.css' {
  const _default: CSSStyleSheet;
  export default _default;
}
