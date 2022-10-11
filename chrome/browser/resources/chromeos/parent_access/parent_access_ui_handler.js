// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ParentAccessUIHandler, ParentAccessUIHandlerRemote} from './parent_access_ui.mojom-webui.js';

/** @type {?ParentAccessUIHandlerRemote} */
let parentAccessUIHandler = null;

/** @return  {!ParentAccessUIHandlerRemote} */
export function getParentAccessUIHandler() {
  if (!parentAccessUIHandler) {
    parentAccessUIHandler = ParentAccessUIHandler.getRemote();
  }
  return parentAccessUIHandler;
}