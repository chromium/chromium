// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ParentAccessParams, ParentAccessUIHandler, ParentAccessUIHandlerRemote} from './parent_access_ui.mojom-webui.js';

/** @type {?ParentAccessUIHandlerRemote} */
let parentAccessUIHandler = null;

/** @type {?{params: !ParentAccessParams}} */
let parentAccessParams = null;

/** @return  {!ParentAccessUIHandlerRemote} */
export function getParentAccessUIHandler() {
  if (!parentAccessUIHandler) {
    parentAccessUIHandler = ParentAccessUIHandler.getRemote();
  }
  return parentAccessUIHandler;
}

/** @return  {!Promise<{params: !ParentAccessParams}>} */
export async function getParentAccessParams() {
  if (!parentAccessParams) {
    parentAccessParams =
        await getParentAccessUIHandler().getParentAccessParams();
  }
  return parentAccessParams;
}

/** @type {!ParentAccessUIHandlerRemote} */
export function setParentAccessUIHandlerForTest(handler) {
  parentAccessUIHandler = handler;
}