// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the root file for the glic app. The exports are used in WebUI tests.

import './main.js';

export type {BrowserProxy} from './browser_proxy.js';
export type {PageHandlerInterface} from './glic_webui.mojom-webui.js';
export {GuestPageType, PageCallbackRouter, PreloadPageCallbackRouter, ZoomAction} from './glic_webui.mojom-webui.js';
export {ObservableValue, Subject} from './observable.js';
export type {WebviewDelegate} from './webview.js';
export {WebviewController, WebviewPersistentState} from './webview.js';
