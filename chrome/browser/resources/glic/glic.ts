// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the root file for the glic app. The exports are used in WebUI tests.

import './main.js';

export type {BrowserProxy} from './browser_proxy.js';
export type {PageHandlerInterface} from './glic.mojom-webui.js';
export {ZoomAction} from './glic.mojom-webui.js';
export type {ObservableSetByTabIdDelegate} from './glic_api_impl/client/glic_api_client.js';
export {IdGenerator, ObservableSetByTabId} from './glic_api_impl/client/glic_api_client.js';
export {GatedSender} from './glic_api_impl/host/gated_sender.js';
export type {ApiHostEmbedder} from './glic_api_impl/host/glic_api_host.js';
export type {RequestMessage} from './glic_api_impl/post_message_transport.js';
export {PostMessageRequestSender, PostMessageRouter, Queue} from './glic_api_impl/post_message_transport.js';
export {ObservableValue, Subject} from './observable.js';
export type {PageType, WebviewDelegate} from './webview.js';
export {matcherForOrigin, urlMatchesAllowedOrigin, WebviewController, WebviewPersistentState} from './webview.js';
