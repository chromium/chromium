// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shopping_tasks.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for retrieving the shopping task for the shopping task module.
 */

export class ShoppingTasksHandlerProxy {
  constructor() {
    /** @type {!shoppingTasks.mojom.ShoppingTasksHandlerRemote} */
    this.handler = shoppingTasks.mojom.ShoppingTasksHandler.getRemote();
  }
}

addSingletonGetter(ShoppingTasksHandlerProxy);
