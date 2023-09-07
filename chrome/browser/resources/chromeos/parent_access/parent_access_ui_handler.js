// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ParentAccessParams, ParentAccessUiHandler, ParentAccessUiHandlerRemote} from './parent_access_ui.mojom-webui.js';

/** @type {?ParentAccessUiHandlerRemote} */
let parentAccessUIHandler = null;

/** @type {?{params: !ParentAccessParams}} */
let parentAccessParams = null;

/** @return  {!ParentAccessUiHandlerRemote} */
export function getParentAccessUiHandler() {
  if (!parentAccessUIHandler) {
    parentAccessUIHandler = ParentAccessUiHandler.getRemote();
  }
  return parentAccessUIHandler;
}

/** @return  {!Promise<{params: !ParentAccessParams}>} */
export async function getParentAccessParams() {
  if (!parentAccessParams) {
    parentAccessParams =
        await getParentAccessUiHandler().getParentAccessParams();
  }
  return parentAccessParams;
}

/** @type {!ParentAccessUiHandlerRemote} */
export function setParentAccessUiHandlerForTest(handler) {
  parentAccessUIHandler = handler;
}
