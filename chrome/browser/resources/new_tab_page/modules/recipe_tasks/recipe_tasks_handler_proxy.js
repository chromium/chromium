// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './recipe_tasks.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for retrieving the recipe task for the recipe task module.
 */

export class RecipeTasksHandlerProxy {
  constructor() {
    /** @type {!recipeTasks.mojom.RecipeTasksHandlerRemote} */
    this.handler = recipeTasks.mojom.RecipeTasksHandler.getRemote();
  }
}

addSingletonGetter(RecipeTasksHandlerProxy);
