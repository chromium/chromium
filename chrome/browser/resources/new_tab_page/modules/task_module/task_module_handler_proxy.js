// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './task_module.mojom-lite.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for retrieving a shopping task for a task module.
 */

/** @type {?taskModule.mojom.TaskModuleHandlerRemote} */
let handler = null;

export class TaskModuleHandlerProxy {
  /** @return {!taskModule.mojom.TaskModuleHandlerRemote} */
  static getHandler() {
    return handler ||
        (handler = taskModule.mojom.TaskModuleHandler.getRemote());
  }

  /** @param {!taskModule.mojom.TaskModuleHandlerRemote} newHandler */
  static setHandler(newHandler) {
    handler = newHandler;
  }

  /** @private */
  constructor() {}
}
