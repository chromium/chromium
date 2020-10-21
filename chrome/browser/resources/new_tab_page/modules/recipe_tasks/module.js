// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../img.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ModuleDescriptor} from '../module_descriptor.js';
import {RecipeTasksHandlerProxy} from './recipe_tasks_handler_proxy.js';

/**
 * @fileoverview Implements the UI of the recipes module. This module shows
 * recently view and related recipes
 */

class RecipeTasksModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-recipe-tasks-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {recipeTasks.mojom.RecipeTask} */
      recipeTask: Object,

      /** @type {boolean} */
      showInfoDialog: Boolean,
    };
  }

  /** @override */
  ready() {
    super.ready();
    /** @type {IntersectionObserver} */
    this.intersectionObserver_ = null;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onRecipeClick_(e) {
    const index = this.$.recipesRepeat.indexForElement(e.target);
    RecipeTasksHandlerProxy.getInstance().handler.onRecipeClicked(index);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onPillClick_(e) {
    const index = this.$.relatedSearchesRepeat.indexForElement(e.target);
    RecipeTasksHandlerProxy.getInstance().handler.onRelatedSearchClicked(index);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  /** @private */
  onCloseClick_() {
    this.showInfoDialog = false;
  }

  /** @private */
  onDomChange_() {
    if (!this.intersectionObserver_) {
      this.intersectionObserver_ = new IntersectionObserver(entries => {
        entries.forEach(({intersectionRatio, target}) => {
          target.style.visibility =
              intersectionRatio < 1 ? 'hidden' : 'visible';
        });
        this.dispatchEvent(new Event('visibility-update'));
      }, {root: this, threshold: 1});
    } else {
      this.intersectionObserver_.disconnect();
    }
    this.shadowRoot.querySelectorAll('.recipe, .pill')
        .forEach(el => this.intersectionObserver_.observe(el));
  }
}

customElements.define(RecipeTasksModuleElement.is, RecipeTasksModuleElement);

/** @return {!Promise<?{element: !HTMLElement, title: string}>} */
async function createModule() {
  const {recipeTask} = await RecipeTasksHandlerProxy.getInstance()
                           .handler.getPrimaryRecipeTask();
  if (!recipeTask) {
    return null;
  }
  const element = new RecipeTasksModuleElement();
  element.recipeTask = recipeTask;
  return {
    element: element,
    title: recipeTask.title,
    actions: {
      info: () => {
        element.showInfoDialog = true;
      },
      dismiss: () => {
        RecipeTasksHandlerProxy.getInstance().handler.dismissRecipeTask(
            recipeTask.name);
        return loadTimeData.getStringF(
            'dismissModuleToastMessage', recipeTask.name);
      },
      restore: () => {
        RecipeTasksHandlerProxy.getInstance().handler.restoreRecipeTask(
            recipeTask.name);
      },
    },
  };
}

/** @type {!ModuleDescriptor} */
export const recipeTasksDescriptor = new ModuleDescriptor(
    /*id=*/ 'recipe_tasks',
    /*heightPx=*/ 206, createModule);
