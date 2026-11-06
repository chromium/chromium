// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bundle entry point. Optimized builds only serve this file, so tests must
// import from here to share module instances (e.g. singletons) with the page.
import './selection_overlay_app.js';

export {ScreenshotBitmapBrowserProxyImpl} from '/lens/screenshot_bitmap_browser_proxy.js';
export {SelectionOverlayBaseHandler} from '/lens/selection_overlay_base_handler.js';
export {create as createExplainFulfillment} from './explain_fulfillment.js';
export type {ExplainFulfillmentElement} from './explain_fulfillment.js';
export {ExplainFulfillment} from './explain_fulfillment.mojom-webui.js';
export type {ExplainFulfillmentInterface} from './explain_fulfillment.mojom-webui.js';
export type {GlicSelectionOverlayElement} from './glic_selection_overlay.js';
export type {InlineFulfillmentHostElement, InlineFulfillmentModule, InlineFulfillmentRemote} from './inline_fulfillment_host.js';
export {renderMarkdown} from './markdown.js';
export {browserProxyFactory, SelectionOverlayPageHandlerRemote, SuggestedActionIcon} from './selection_overlay.mojom-webui.js';
export type {SelectionOverlayPageRemote, SuggestedAction, SuggestedActionsListenerRemote} from './selection_overlay.mojom-webui.js';
export {SelectionOverlayBaseHandlerImpl} from './selection_overlay_base_handler_impl.js';
