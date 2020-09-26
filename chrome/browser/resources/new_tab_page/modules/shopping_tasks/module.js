// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../img.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ModuleDescriptor} from '../module_descriptor.js';
import {ShoppingTasksHandlerProxy} from './shopping_tasks_handler_proxy.js';

/**
 * @fileoverview Implements the UI of the shopping tasks module. This module
 * shows a currently active shopping search journey and facilitates the user to
 * that search journey up again.
 */

class ShoppingTasksModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-shopping-tasks-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {shoppingTasks.mojom.ShoppingTask} */
      shoppingTask: Object,
    };
  }
}

customElements.define(
    ShoppingTasksModuleElement.is, ShoppingTasksModuleElement);

/** @return {!Promise<?{element: !HTMLElement, title: string}>} */
async function createModule() {
  const {shoppingTask} = await ShoppingTasksHandlerProxy.getInstance()
                             .handler.getPrimaryShoppingTask();
  if (!shoppingTask) {
    return null;
  }
  const element = new ShoppingTasksModuleElement();
  element.shoppingTask = shoppingTask;
  return {
    element: element,
    title: shoppingTask.title,
  };
}

/** @type {!ModuleDescriptor} */
export const shoppingTasksDescriptor = new ModuleDescriptor(
    /*id=*/ 'shopping_tasks',
    /*name=*/ 'Shopping Tasks',
    /*heightPx=*/ 270, createModule);
