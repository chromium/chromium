// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Convenience module to bind to initialize a PageHandler
 * remote i.e. a PageHandler that we can use to talk to the browser.
 */
import {PageHandler, PageHandlerRemote} from '/ash/webui/system_extensions_internals_ui/mojom/system_extensions_internals_ui.mojom-webui.js';

export const pageHandler = PageHandler.getRemote();
