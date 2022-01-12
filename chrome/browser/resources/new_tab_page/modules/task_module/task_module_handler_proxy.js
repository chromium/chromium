// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TaskModuleHandler, TaskModuleHandlerRemote} from '../../task_module.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for retrieving a shopping task for a task module.
 */

/** @type {?TaskModuleHandlerRemote} */
let handler = null;

export class TaskModuleHandlerProxy {
  /** @return {!TaskModuleHandlerRemote} */
  static getHandler() {
    return handler || (handler = TaskModuleHandler.getRemote());
  }

  /** @param {!TaskModuleHandlerRemote} newHandler */
  static setHandler(newHandler) {
    handler = newHandler;
  }

  /** @private */
  constructor() {}
}
