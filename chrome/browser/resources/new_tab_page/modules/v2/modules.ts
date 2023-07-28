// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement, TemplateInstanceBase, templatize} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {NewTabPageProxy} from '../../new_tab_page_proxy.js';
import {WindowProxy} from '../../window_proxy.js';
import {ModuleRegistry} from '../module_registry.js';
import {ModuleInstance, ModuleWrapperElement} from '../module_wrapper.js';

import {getTemplate} from './modules.html.js';

export interface NamedWidth {
  name: string;
  value: number;
}

export const SUPPORTED_MODULE_WIDTHS: NamedWidth[] = [
  {name: 'narrow', value: 312},
  {name: 'medium', value: 360},
  {name: 'wide', value: 728},
];

export const MAX_COLUMN_COUNT = 5;

interface QueryDetails {
  maxWidth: number;
  query: string;
}

/* Derived from 5 * narrow module width + 4 * wrapper gap width. */
const CONTAINER_MAX_WIDTH = 1592;

const CONTAINER_GAP_WIDTH = 8;

const MARGIN_WIDTH = 48;

export type UndoActionEvent =
    CustomEvent<{message: string, restoreCallback?: () => void}>;
export type DismissModuleInstanceEvent = UndoActionEvent;
export type DisableModuleEvent = UndoActionEvent;

declare global {
  interface HTMLElementEventMap {
    'disable-module': DisableModuleEvent;
    'dismiss-module-instance': DismissModuleInstanceEvent;
  }
}

export interface ModulesV2Element {
  $: {
    container: HTMLElement,
    undoToast: CrToastElement,
    undoToastMessage: HTMLElement,
  };
}

/** Container for the NTP modules. */
export class ModulesV2Element extends PolymerElement {
  static get is() {
    return 'ntp-modules-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabledModules_: {
        type: Object,
        observer: 'onDisabledModulesChange_',
        value: () => ({all: true, ids: []}),
      },

      /** Data about the most recent un-doable action. */
      undoData_: {
        type: Object,
        value: null,
      },
    };
  }

  private disabledModules_: {all: boolean, ids: string[]};
  private eventTracker_: EventTracker = new EventTracker();
  private undoData_: {message: string, undo?: () => void}|null;
  private setDisabledModulesListenerId_: number|null = null;
  private containerObserver_: MutationObserver|null = null;
  private templateInstances_: TemplateInstanceBase[] = [];

  override connectedCallback() {
    super.connectedCallback();

    this.setDisabledModulesListenerId_ =
        NewTabPageProxy.getInstance()
            .callbackRouter.setDisabledModules.addListener(
                (all: boolean, ids: string[]) => {
                  this.disabledModules_ = {all, ids};
                });
    NewTabPageProxy.getInstance().handler.updateDisabledModules();

    const widths: Set<number> = new Set();
    for (let i = 0; i < SUPPORTED_MODULE_WIDTHS.length; i++) {
      const namedWidth = SUPPORTED_MODULE_WIDTHS[i];
      for (let u = 1; u <= MAX_COLUMN_COUNT - i; u++) {
        const width = (namedWidth.value * u) + (CONTAINER_GAP_WIDTH * (u - 1));
        if (width <= CONTAINER_MAX_WIDTH) {
          widths.add(width);
        }
      }
    }
    // Widths must be deduped and sorted to ensure the min-width and max-with
    // media features in the queries produced below are correctly generated.
    const thresholds = [...widths];
    thresholds.sort((i, j) => i - j);

    const queries: QueryDetails[] = [];
    for (let i = 1; i < thresholds.length - 1; i++) {
      queries.push({
        maxWidth: (thresholds[i + 1] - 1),
        query: `(min-width: ${
            thresholds[i] + 2 * MARGIN_WIDTH}px) and (max-width: ${
            thresholds[i + 1] - 1 + (2 * MARGIN_WIDTH)}px)`,
      });
    }
    queries.splice(0, 0, {
      maxWidth: thresholds[0],
      query: `(max-width: ${thresholds[0] - 1 + (2 * MARGIN_WIDTH)}px)`,
    });
    queries.push({
      maxWidth: thresholds[thresholds.length - 1],
      query: `(min-width: ${
          thresholds[thresholds.length - 1] + (2 * MARGIN_WIDTH)}px)`,
    });

    // Produce media queries with relevant view thresholds at which module
    // instance optimal widths should be re-evaluated.
    queries.forEach(details => {
      const query = WindowProxy.getInstance().matchMedia(details.query);
      this.eventTracker_.add(query, 'change', (e: MediaQueryListEvent) => {
        if (e.matches) {
          this.updateContainerAndChildrenStyles_(details.maxWidth);
        }
      });
    });

    this.eventTracker_.add(window, 'keydown', this.onWindowKeydown_.bind(this));

    this.containerObserver_ = new MutationObserver(() => {
      this.updateContainerAndChildrenStyles_();
    });
    this.containerObserver_!.observe(this.$.container, {childList: true});
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.setDisabledModulesListenerId_);
    NewTabPageProxy.getInstance().callbackRouter.removeListener(
        this.setDisabledModulesListenerId_);

    this.eventTracker_.removeAll();

    this.containerObserver_!.disconnect();
  }

  override ready() {
    super.ready();

    this.updateStyles({
      '--container-gap': `${CONTAINER_GAP_WIDTH}px`,
    });

    this.loadModules_();
  }

  private moduleDisabled_(
      disabledModules: {all: true, ids: string[]},
      instance: ModuleInstance): boolean {
    return disabledModules.all ||
        disabledModules.ids.includes(instance.descriptor.id);
  }

  private async loadModules_(): Promise<void> {
    const modulesIdNames =
        (await NewTabPageProxy.getInstance().handler.getModulesIdNames()).data;
    const modules =
        await ModuleRegistry.getInstance().initializeModulesHavingIds(
            modulesIdNames.map(m => m.id),
            loadTimeData.getInteger('modulesLoadTimeout'));
    if (modules) {
      NewTabPageProxy.getInstance().handler.onModulesLoadedWithData(
          modules.map(module => module.descriptor.id));

      const template = this.shadowRoot!.querySelector('template')!;
      const moduleWrapperConstructor:
          {new (_: Object): TemplateInstanceBase&HTMLElement} =
              templatize(template, this, {
                parentModel: true,
                forwardHostProp: this.forwardHostProp_,
                instanceProps: {item: true},
              }) as {new (): TemplateInstanceBase & HTMLElement};
      this.templateInstances_ =
          modules
              .map(module => {
                return module.elements.map(element => {
                  return {
                    element,
                    descriptor: module.descriptor,
                  };
                });
              })
              .flat()
              .map(instance => {
                return new moduleWrapperConstructor({item: instance});
              });
      this.templateInstances_.map(t => t.children[0] as HTMLElement)
          .forEach(wrapperElement => {
            this.$.container.appendChild(wrapperElement);
          });

      chrome.metricsPrivate.recordSmallCount(
          'NewTabPage.Modules.LoadedModulesCount', modules.length);
      // TODO(crbug.com/1444758): Add module instances count metric.
      this.dispatchEvent(new Event('modules-loaded'));
    }
  }

  private forwardHostProp_(property: string, value: any) {
    this.templateInstances_.forEach(instance => {
      instance.forwardHostProp(property, value);
    });
  }

  private updateContainerAndChildrenStyles_(availableWidth?: number) {
    if (typeof availableWidth === 'undefined') {
      availableWidth = Math.min(
          document.body.clientWidth - 2 * MARGIN_WIDTH, CONTAINER_MAX_WIDTH);
    }

    const moduleWrappers =
        Array.from(this.shadowRoot!.querySelectorAll(
            'ntp-module-wrapper:not([hidden])')) as ModuleWrapperElement[];
    if (moduleWrappers.length === 0) {
      return;
    }

    this.updateStyles({'--container-max-width': `${availableWidth}px`});

    const clamp = (min: number, val: number, max: number) =>
        Math.max(min, Math.min(val, max));
    const rowMaxInstanceCount = clamp(
        1,
        Math.floor(
            (availableWidth + CONTAINER_GAP_WIDTH) /
            (CONTAINER_GAP_WIDTH + SUPPORTED_MODULE_WIDTHS[0].value)),
        MAX_COLUMN_COUNT);

    let index = 0;
    while (index < moduleWrappers.length) {
      const instances = moduleWrappers.slice(index, index + rowMaxInstanceCount)
                            .map(w => w.module);
      let namedWidth = SUPPORTED_MODULE_WIDTHS[0];
      for (let i = 1; i < SUPPORTED_MODULE_WIDTHS.length; i++) {
        if (Math.floor(
                (availableWidth -
                 (CONTAINER_GAP_WIDTH * (instances.length - 1))) /
                SUPPORTED_MODULE_WIDTHS[i].value) < instances.length) {
          break;
        }
        namedWidth = SUPPORTED_MODULE_WIDTHS[i];
      }

      instances.slice(0, instances.length).forEach(instance => {
        // The `format` attribute is leveraged by modules whose layout should
        // change based on the available width.
        instance.element.setAttribute('format', namedWidth.name);
        instance.element.style.width = `${namedWidth.value}px`;
      });

      index += instances.length;
    }
  }

  private onDisableModule_(e: DisableModuleEvent) {
    const id = (e.target! as ModuleWrapperElement).module.descriptor.id;
    const restoreCallback = e.detail.restoreCallback;
    this.undoData_ = {
      message: e.detail.message,
      undo: () => {
        if (restoreCallback) {
          restoreCallback();
        }
        NewTabPageProxy.getInstance().handler.setModuleDisabled(id, false);
        chrome.metricsPrivate.recordSparseValueWithPersistentHash(
            'NewTabPage.Modules.Enabled', id);
        chrome.metricsPrivate.recordSparseValueWithPersistentHash(
            'NewTabPage.Modules.Enabled.Toast', id);
      },
    };

    NewTabPageProxy.getInstance().handler.setModuleDisabled(id, true);
    this.$.undoToast.show();
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        'NewTabPage.Modules.Disabled', id);
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        'NewTabPage.Modules.Disabled.ModuleRequest', id);
  }

  private onDisabledModulesChange_() {
    this.updateContainerAndChildrenStyles_();
  }

  /**
   * @param e Event notifying a module instance was dismissed. Contains the
   *     message to show in the toast.
   */
  private onDismissModuleInstance_(e: DismissModuleInstanceEvent) {
    const wrapper = (e.target! as ModuleWrapperElement);
    const index = Array.from(wrapper.parentNode!.children).indexOf(wrapper);
    wrapper.remove();

    const restoreCallback = e.detail.restoreCallback;
    this.undoData_ = {
      message: e.detail.message,
      undo: restoreCallback ?
          () => {
            this.$.container.insertBefore(
                wrapper, this.$.container.childNodes[index]);
            restoreCallback();
          } :
          undefined,
    };

    // Notify the user.
    this.$.undoToast.show();
  }

  private onUndoButtonClick_() {
    if (!this.undoData_) {
      return;
    }

    // Restore to the previous state.
    this.undoData_.undo!();
    // Notify the user.
    this.$.undoToast.hide();
    this.undoData_ = null;
  }

  private onWindowKeydown_(e: KeyboardEvent) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.key === 'z') {
      this.onUndoButtonClick_();
    }
  }
}

customElements.define(ModulesV2Element.is, ModulesV2Element);
