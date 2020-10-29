// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './task_module.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for retrieving a shopping task for a task module.
 */

export class TaskModuleHandlerProxy {
  constructor() {
    /** @type {!taskModule.mojom.TaskModuleHandlerRemote} */
    this.handler = taskModule.mojom.TaskModuleHandler.getRemote();
  }
}

addSingletonGetter(TaskModuleHandlerProxy);
