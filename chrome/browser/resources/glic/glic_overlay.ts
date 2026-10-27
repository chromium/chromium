// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the root file for the glic overlay app. The exports are used in WebUI
// tests.

import './glic_overlay_app.js';

export {browserProxyFactory, ErrorPanelType, GlicOverlayPageCallbackRouter, GlicOverlayPageHandlerFactory, GlicOverlayPageHandlerRemote, LoadingStyle} from './glic_overlay.mojom-webui.js';
export type {BrowserProxy, GlicOverlayPageRemote, OverlayState} from './glic_overlay.mojom-webui.js';
export {GlicOverlayElement} from './glic_overlay_app.js';
