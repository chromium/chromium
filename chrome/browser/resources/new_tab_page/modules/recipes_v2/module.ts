// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin} from '../../i18n_setup.js';
import {Recipe} from '../../recipes.mojom-webui.js';
import {InfoDialogElement} from '../info_dialog.js';
import {ModuleDescriptorV2, ModuleHeight} from '../module_descriptor.js';
import {RecipesHandlerProxy} from '../recipes/recipes_handler_proxy.js';

import {getTemplate} from './module.html.js';

export interface RecipesModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
    recipesRepeat: DomRepeat,
  };
}

export class RecipesModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-recipes-module-redesigned';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      recipes: Array,
    };
  }

  recipes: Recipe[];

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }
}

customElements.define(RecipesModuleElement.is, RecipesModuleElement);

async function createModule(): Promise<HTMLElement> {
  const {task} = await RecipesHandlerProxy.getHandler().getPrimaryTask();
  const element = new RecipesModuleElement();
  element.recipes = (task && task.recipes) || [];
  return element;
}

export const recipeTasksDescriptor: ModuleDescriptorV2 = new ModuleDescriptorV2(
    /*id=*/ 'recipe_tasks',
    /*height*/ ModuleHeight.TALL, createModule);
