// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as geicMojom from './geic.mojom-webui.js';

export * from './geic.mojom-webui.js';

declare global {
  interface Window {
    geic?: {
      mojom?: Record<string, unknown>,
    };
  }
}

const allExports: Record<string, unknown> = {
  ...geicMojom,
};

// Attach bindings to the global window object under window.geic.mojom.*
// Define window.geic as a non-configurable, non-writable property to prevent
// prototype pollution, pre-existing setters, or proxy trapping.
Object.defineProperty(window, 'geic', {
  value: {
    mojom: {
      ...allExports,
    },
  },
  writable: false,
  configurable: false,
  enumerable: true,
});

// Signals to a GE web app that may have loaded before this deferred module
// executed. The GE web app also checks window.geic.mojom synchronously first,
// since a listener registered after this dispatch would never fire.
window.dispatchEvent(new CustomEvent('geic-mojom-ready', {detail: allExports}));
