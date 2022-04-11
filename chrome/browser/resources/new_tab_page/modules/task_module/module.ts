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
import {RelatedSearch, Task, TaskItem} from '../../task_module.mojom-webui.js';
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
      task: Object,

      title_: {
        type: String,
        computed: 'computeTitle_(task)',
      },

      dismissName_: {
        type: String,
        computed: 'computeDismissName_(task)',
      },

      disableName_: {
        type: String,
        computed: 'computeDisableName_()',
      },
    };
  }

  task: Task;
  private title_: string;
  private dismissName_: string;
  private disableName_: string;

  private intersectionObserver_: IntersectionObserver|null = null;

  private computeTitle_(): string {
    return loadTimeData.getString('modulesRecipeTasksSentence');
  }

  private computeDismissName_(): string {
    return loadTimeData.getString('modulesRecipeTasksLowerThese');
  }

  private computeDisableName_(): string {
    return loadTimeData.getString('modulesRecipeTasksLower');
  }

  private onTaskItemClick_(e: DomRepeatEvent<TaskItem>) {
    const index = e.model.index;
    TaskModuleHandlerProxy.getHandler().onTaskItemClicked(index);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  private onPillClick_(e: DomRepeatEvent<RelatedSearch>) {
    const index = e.model.index;
    TaskModuleHandlerProxy.getHandler().onRelatedSearchClicked(index);
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onDismissButtonClick_() {
    TaskModuleHandlerProxy.getHandler().dismissTask(this.task.name);
    const taskName = loadTimeData.getString('modulesRecipeTasksSentence');
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
    TaskModuleHandlerProxy.getHandler().restoreTask(this.task.name);
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

async function createModule(): Promise<HTMLElement|null> {
  const {task} = await TaskModuleHandlerProxy.getHandler().getPrimaryTask();
  if (!task) {
    return null;
  }
  const element = new TaskModuleElement();
  element.task = task;
  return element;
}

export const recipeTasksDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'recipe_tasks',
    /*name=*/ loadTimeData.getString('modulesRecipeTasksSentence'),
    createModule);
