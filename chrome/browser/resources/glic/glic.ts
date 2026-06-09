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
export type {WebClientHost} from './glic_api_impl/request_types.js';
export {WebClientDef, WebClientHostDef} from './glic_api_impl/request_types.js';
export type {InterfaceDef, InterfaceDefMethods} from './glic_api_impl/transport/messaging.js';
export {defInterface, defMessage} from './glic_api_impl/transport/messaging.js';
export type {ErrorCodec, PendingReceiver, PendingRemote, PostMessageHandler, PostMessageLifecycleObserver, PostMessageReceiver, PostMessageRemote, PostMessageSender, RequestMessage, TransferableException} from './glic_api_impl/transport/post_message_transport.js';
export {createBidirectionalPostMessageTransport, InverseSet, ON_PIPE_CLOSED, PostMessageReceiverImpl, PostMessageRemoteImpl, PostMessageRequestReceiver, PostMessageRequestSender, PostMessageRouterImpl, Queue} from './glic_api_impl/transport/post_message_transport.js';
export {ObservableValue, Subject} from './observable.js';
export type {PageType, WebviewDelegate} from './webview.js';
export {matcherForOrigin, urlMatchesAllowedOrigin, urlMatchesApiAllowedOrigin, WebviewController, WebviewPersistentState} from './webview.js';
