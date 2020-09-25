// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  constructor() {
    super();
    ShoppingTasksHandlerProxy.getInstance()
        .handler.getPrimaryShoppingTask()
        .then(data => {
          // TODO(crbug.com/1130855): Do something useful with the data.
          console.log(data);
        });
  }
}

customElements.define(
    ShoppingTasksModuleElement.is, ShoppingTasksModuleElement);

/** @type {!ModuleDescriptor} */
export const shoppingTasksDescriptor = new ModuleDescriptor(
    /*id=*/ 'shopping_tasks',
    /*name=*/ 'Shopping Tasks',
    /*heightPx=*/ 260, () => Promise.resolve({
      element: new ShoppingTasksModuleElement(),
      title: 'Shopping Tasks',
    }));
