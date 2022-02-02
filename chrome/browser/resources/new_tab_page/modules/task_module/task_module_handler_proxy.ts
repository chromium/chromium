// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TaskModuleHandler, TaskModuleHandlerRemote} from '../../task_module.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for retrieving a shopping task for a task module.
 */

let handler: TaskModuleHandlerRemote|null = null;

export class TaskModuleHandlerProxy {
  static getHandler(): TaskModuleHandlerRemote {
    return handler || (handler = TaskModuleHandler.getRemote());
  }

  static setHandler(newHandler: TaskModuleHandlerRemote) {
    handler = newHandler;
  }

  private constructor() {}
}
