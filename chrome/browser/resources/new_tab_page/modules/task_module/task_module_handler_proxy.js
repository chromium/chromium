// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './task_module.mojom-lite.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for retrieving a shopping task for a task module.
 */

/** @type {TaskModuleHandlerProxy} */
let instance = null;

export class TaskModuleHandlerProxy {
  /** @return {!TaskModuleHandlerProxy} */
  static getInstance() {
    return instance || (instance = new TaskModuleHandlerProxy());
  }

  /** @param {TaskModuleHandlerProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @type {!taskModule.mojom.TaskModuleHandlerRemote} */
    this.handler = taskModule.mojom.TaskModuleHandler.getRemote();
  }
}
