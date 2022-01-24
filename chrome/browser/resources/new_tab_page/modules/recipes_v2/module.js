// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {TaskModuleHandlerProxy} from '../task_module/task_module_handler_proxy.js';

class RecipeModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-recipes-module-redesigned';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!taskModule.mojom.Task} */
      task: Object,
    };
  }
}

customElements.define(RecipeModuleElement.is, RecipeModuleElement);

/** @return {!Promise<?HTMLElement>} */
async function createModule() {
  const {task} = await TaskModuleHandlerProxy.getHandler().getPrimaryTask(
      taskModule.mojom.TaskModuleType.kRecipe);
  if (!task) {
    return null;
  }
  const element = new RecipeModuleElement();
  element.task = task;
  return element;
}

/** @type {!ModuleDescriptor} */
export const recipeTasksDescriptor = new ModuleDescriptor(
    /*id=*/ 'recipe_tasks',
    /*name=*/ loadTimeData.getString('modulesRecipeTasksSentence'),
    createModule);
