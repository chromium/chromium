// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin, loadTimeData} from '../../i18n_setup.js';
import {RelatedSearch, Task, TaskItem, TaskModuleType} from '../../task_module.mojom-webui.js';
import {InfoDialogElement} from '../info_dialog.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {getTemplate} from './module.html.js';
import {TaskModuleHandlerProxy} from './task_module_handler_proxy.js';

export interface TaskModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
    relatedSearchesRepeat: DomRepeat,
    taskItemsRepeat: DomRepeat,
  };
}

/**
 * Implements the UI of a task module. This module shows a currently active task
 * search journey and provides a way for the user to continue that search
 * journey.
 */
export class TaskModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-task-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      taskModuleType: {
        type: Number,
        observer: 'onTaskModuleTypeChange_',
      },

      task: Object,

      title_: {
        type: String,
        computed: 'computeTitle_(taskModuleType, task)',
      },

      dismissName_: {
        type: String,
        computed: 'computeDismissName_(taskModuleType, task)',
      },

      disableName_: {
        type: String,
        computed: 'computeDisableName_(taskModuleType)',
      },
    };
  }

  taskModuleType: TaskModuleType;
  task: Task;
  private title_: string;
  private dismissName_: string;
  private disableName_: string;

  private intersectionObserver_: IntersectionObserver|null = null;

  private computeTitle_(): string {
    switch (this.taskModuleType) {
      case TaskModuleType.kRecipe:
        return loadTimeData.getString('modulesRecipeTasksSentence');
      case TaskModuleType.kShopping:
        return this.task.title;
      default:
        return '';
    }
  }

  private computeDismissName_(): string {
    switch (this.taskModuleType) {
      case TaskModuleType.kRecipe:
        return loadTimeData.getString('modulesRecipeTasksLowerThese');
      case TaskModuleType.kShopping:
        return this.task.name;
      default:
        return '';
    }
  }

  private computeDisableName_(): string {
    switch (this.taskModuleType) {
      case TaskModuleType.kRecipe:
        return loadTimeData.getString('modulesRecipeTasksLower');
      case TaskModuleType.kShopping:
        return loadTimeData.getString('modulesShoppingTasksLower');
      default:
        return '';
    }
  }

  private isRecipe_(): boolean {
    return this.taskModuleType === TaskModuleType.kRecipe;
  }

  private isShopping_(): boolean {
    return this.taskModuleType === TaskModuleType.kShopping;
  }

  private onTaskModuleTypeChange_() {
    switch (this.taskModuleType) {
      case TaskModuleType.kRecipe:
        this.toggleAttribute('recipe');
        break;
      case TaskModuleType.kShopping:
        this.toggleAttribute('shopping');
        break;
    }
  }

  private onTaskItemClick_(e: DomRepeatEvent<TaskItem>) {
    const index = e.model.index;
    TaskModuleHandlerProxy.getHandler().onTaskItemClicked(
        this.taskModuleType, index);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  private onPillClick_(e: DomRepeatEvent<RelatedSearch>) {
    const index = e.model.index;
    TaskModuleHandlerProxy.getHandler().onRelatedSearchClicked(
        this.taskModuleType, index);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onDismissButtonClick_() {
    TaskModuleHandlerProxy.getHandler().dismissTask(
        this.taskModuleType, this.task.name);
    let taskName = '';
    switch (this.taskModuleType) {
      case TaskModuleType.kRecipe:
        taskName = loadTimeData.getString('modulesRecipeTasksSentence');
        break;
      case TaskModuleType.kShopping:
        taskName = this.task.name;
        break;
    }
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF('dismissModuleToastMessage', taskName),
        restoreCallback: this.onRestore_.bind(this),
      },
    }));
  }

  private onDisableButtonClick_() {
    this.dispatchEvent(new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage', this.disableName_),
      },
    }));
  }

  private onRestore_() {
    TaskModuleHandlerProxy.getHandler().restoreTask(
        this.taskModuleType, this.task.name);
  }

  private onDomChange_() {
    if (!this.intersectionObserver_) {
      this.intersectionObserver_ = new IntersectionObserver(entries => {
        entries.forEach(({intersectionRatio, target}) => {
          (target as HTMLElement).style.visibility =
              intersectionRatio < 1 ? 'hidden' : 'visible';
        });
        this.dispatchEvent(new Event('visibility-update'));
      }, {root: this, threshold: 1});
    } else {
      this.intersectionObserver_.disconnect();
    }
    this.shadowRoot!.querySelectorAll('.task-item, .pill')
        .forEach(el => this.intersectionObserver_!.observe(el));
  }
}

customElements.define(TaskModuleElement.is, TaskModuleElement);

async function createModule(taskModuleType: TaskModuleType):
    Promise<HTMLElement|null> {
  const {task} =
      await TaskModuleHandlerProxy.getHandler().getPrimaryTask(taskModuleType);
  if (!task) {
    return null;
  }
  const element = new TaskModuleElement();
  element.taskModuleType = taskModuleType;
  element.task = task;
  return element;
}

export const recipeTasksDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'recipe_tasks',
    /*name=*/ loadTimeData.getString('modulesRecipeTasksSentence'),
    createModule.bind(null, TaskModuleType.kRecipe));

export const shoppingTasksDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'shopping_tasks',
    /*name=*/ loadTimeData.getString('modulesShoppingTasksSentence'),
    createModule.bind(null, TaskModuleType.kShopping));
