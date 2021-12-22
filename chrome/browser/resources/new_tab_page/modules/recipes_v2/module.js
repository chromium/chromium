// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {ModuleDescriptorV2, ModuleHeight} from '../module_descriptor.js';
import {TaskModuleHandlerProxy} from '../task_module/task_module_handler_proxy.js';

/**
 * @polymer
 * @extends {PolymerElement}
 */
class RecipeModuleElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-recipes-module-redesigned';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<!taskModule.mojom.TaskItem>} */
      recipes: Array,
    };
  }
}

customElements.define(RecipeModuleElement.is, RecipeModuleElement);

/** @return {!Promise<!HTMLElement>} */
async function createModule() {
  const {task} = await TaskModuleHandlerProxy.getHandler().getPrimaryTask(
      taskModule.mojom.TaskModuleType.kRecipe);
  const element = new RecipeModuleElement();
  element.recipes = (task && task.taskItems) || [];
  return element;
}

/** @type {!ModuleDescriptorV2} */
export const recipeTasksDescriptor = new ModuleDescriptorV2(
    /*id=*/ 'recipe_tasks',
    /*name=*/ loadTimeData.getString('modulesRecipeTasksSentence'),
    /*height*/ ModuleHeight.TALL, createModule);
