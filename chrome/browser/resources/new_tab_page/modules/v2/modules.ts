// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../../i18n_setup.js';
import {recordOccurence as recordOccurrence} from '../../metrics_utils.js';
import type {ModuleIdName, PageCallbackRouter, PageHandlerRemote} from '../../new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from '../../new_tab_page_proxy.js';
import {WindowProxy} from '../../window_proxy.js';
import type {Module} from '../module_descriptor.js';
import {ModuleRegistry} from '../module_registry.js';
import type {ModuleInstance, ModuleWrapperElement} from '../module_wrapper.js';

import {getCss} from './modules.css.js';
import {getHtml} from './modules.html.js';

export interface NamedWidth {
  name: string;
  value: number;
}

export const SUPPORTED_MODULE_WIDTHS: NamedWidth[] = [
  {name: 'narrow', value: 312},
  {name: 'medium', value: 360},
  {name: 'wide', value: 728},
];

interface QueryDetails {
  maxWidth: number;
  query: string;
}

const CONTAINER_GAP_WIDTH = 8;

const MARGIN_WIDTH = 48;

const METRIC_NAME_MODULE_DISABLED = 'NewTabPage.Modules.Disabled';

export type UndoActionEvent =
    CustomEvent<{message: string, restoreCallback?: () => void}>;
export type DismissModuleElementEvent = UndoActionEvent;
export type DismissModuleInstanceEvent = UndoActionEvent;
export type DisableModuleEvent = UndoActionEvent;

declare global {
  interface HTMLElementEventMap {
    'disable-module': DisableModuleEvent;
    'dismiss-module-instance': DismissModuleInstanceEvent;
    'dismiss-module-element': DismissModuleElementEvent;
  }
}

export interface ModulesElement {
  $: {
    container: HTMLElement,
    undoToast: CrToastElement,
    undoToastMessage: HTMLElement,
  };
}

function createModuleInstances(module: Module): ModuleInstance[] {
  return module.elements.map(element => {
    return {
      element,
      descriptor: module.descriptor,
      initialized: false,
      impressed: false,
    };
  });
}

/** Container for the NTP modules. */
export class ModulesElement extends CrLitElement {
  static get is() {
    return 'ntp-modules';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      modulesShownToUser: {
        type: Boolean,
        notify: true,
      },
      moduleInstances_: {type: Array},
      disabledModules_: {type: Object},
      /** Data about the most recent un-doable action. */
      undoData_: {type: Object},
    };
  }

  accessor modulesShownToUser: boolean = false;
  private waitToLoadModules_: boolean =
      loadTimeData.getBoolean('waitToLoadModules');
  accessor moduleInstances_: ModuleInstance[] = [];
  accessor disabledModules_:
      {all: boolean, ids: string[]} = {all: true, ids: []};
  protected accessor undoData_: {message: string, undo?: () => void}|null =
      null;

  private maxColumnCount_: number =
      loadTimeData.getInteger('modulesMaxColumnCount');
  private availableWidth_: number = 0;
  private containerMaxWidth_: number;
  private eventTracker_: EventTracker = new EventTracker();
  private setDisabledModulesListenerId_: number|null = null;
  private setModulesLoadableListenerId_: number|null = null;

  private availableModulesIds_: Set<string>|null = null;
  private moduleLoadPromise_: Promise<void>|null = null;
  // TODO(crbug.com/385174675): Remove |modulesReloadable_| flag when safe.
  // Otherwise, when Microsoft modules are enabled ToT, the current
  // behavior gated by |modulesReloadable_| should become the default module
  // loading behavior.
  private modulesReloadable_: boolean =
      loadTimeData.getBoolean('modulesReloadable');
  private modulesLoadInitiated_: boolean = false;

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();

    const widths: Set<number> = new Set();
    for (let i = 0; i < SUPPORTED_MODULE_WIDTHS.length; i++) {
      const namedWidth = SUPPORTED_MODULE_WIDTHS[i];
      for (let u = 1; u <= this.maxColumnCount_ - i; u++) {
        const width = (namedWidth.value * u) + (CONTAINER_GAP_WIDTH * (u - 1));
        if (width <= this.containerMaxWidth_) {
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

    this.setDisabledModulesListenerId_ =
        this.callbackRouter_.setDisabledModules.addListener(
            (all: boolean, ids: string[]) => {
              this.disabledModules_ = {all, ids};
            });
    this.pageHandler_.updateDisabledModules();

    this.setModulesLoadableListenerId_ =
        this.callbackRouter_.setModulesLoadable.addListener(() => {
          if (this.waitToLoadModules_) {
            this.waitToLoadModules_ = false;
            this.moduleLoadPromise_ = this.loadModules_();
          } else if (this.modulesReloadable_) {
            this.handleModuleEnablement_(this.disabledModules_);
          }
        });
    if (this.waitToLoadModules_) {
      this.pageHandler_.updateModulesLoadable();
    } else {
      this.moduleLoadPromise_ = this.loadModules_();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.setDisabledModulesListenerId_);
    this.callbackRouter_.removeListener(this.setDisabledModulesListenerId_);
    assert(this.setModulesLoadableListenerId_);
    this.callbackRouter_.removeListener(this.setModulesLoadableListenerId_);

    this.eventTracker_.removeAll();

  }

  override firstUpdated() {
    this.style.setProperty('--container-gap', `${CONTAINER_GAP_WIDTH}px`);

    this.containerMaxWidth_ =
        this.maxColumnCount_ * SUPPORTED_MODULE_WIDTHS[0].value +
        (this.maxColumnCount_ - 1) * CONTAINER_GAP_WIDTH;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    this.availableWidth_ = Math.min(
        document.body.clientWidth - 2 * MARGIN_WIDTH, this.containerMaxWidth_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('moduleInstances_') ||
        changedProperties.has('disabledModules_')) {
      this.updateContainerAndChildrenStyles_(this.availableWidth_);
    }
  }

  get pageHandler_(): PageHandlerRemote {
    return NewTabPageProxy.getInstance().handler;
  }

  get callbackRouter_(): PageCallbackRouter {
    return NewTabPageProxy.getInstance().callbackRouter;
  }

  protected moduleDisabled_(instance: ModuleInstance): boolean {
    return this.disabledModules_.all ||
        this.disabledModules_.ids.includes(instance.descriptor.id);
  }

  /**
   * Initializes the module container by loading all currently enabled modules.
   * This method uses the `ModuleRegistry` to determine which modules to load
   * and is called only when the container is empty.
   */
  private async loadModules_(): Promise<void> {
    this.modulesLoadInitiated_ = true;
    const modulesIdNames = (await this.pageHandler_.getModulesIdNames()).data;
    const modules =
        await ModuleRegistry.getInstance().initializeModulesHavingIds(
            modulesIdNames.map((m: ModuleIdName) => m.id),
            loadTimeData.getInteger('modulesLoadTimeout'));
    if (modules) {
      this.pageHandler_.onModulesLoadedWithData(
          modules.map(module => module.descriptor.id));

      this.moduleInstances_ = modules
                                  .map(module => {
                                    return createModuleInstances(module);
                                  })
                                  .flat();

      this.recordInitialLoadMetrics_(modules, modulesIdNames);
      this.dispatchEvent(new Event('modules-loaded'));
    }
  }

  private recordInitialLoadMetrics_(
      modules: Module[], modulesIdNames: ModuleIdName[]) {
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.Modules.LoadedModulesCount', modules.length);
    modulesIdNames.forEach(({id}) => {
      chrome.metricsPrivate.recordBoolean(
          `NewTabPage.Modules.EnabledOnNTPLoad.${id}`,
          !this.disabledModules_.all &&
              !this.disabledModules_.ids.includes(id));
    });
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.Modules.InstanceCount', this.moduleInstances_.length);
    chrome.metricsPrivate.recordBoolean(
        'NewTabPage.Modules.VisibleOnNTPLoad', !this.disabledModules_.all);
    this.recordModuleLoadedWithModules_(/*onNtpLoad=*/ true);
  }

  private recordModuleLoadedWithModules_(onNtpLoad: boolean) {
    const moduleDescriptorIds = [...new Set(
        this.moduleInstances_.map(instance => instance.descriptor.id))];

    const histogramBase = onNtpLoad ? 'NewTabPage.Modules.LoadedWith' :
                                      'NewTabPage.Modules.ReloadedWith';

    for (const moduleDescriptorId of moduleDescriptorIds) {
      moduleDescriptorIds.forEach(id => {
        if (id !== moduleDescriptorId) {
          chrome.metricsPrivate.recordSparseValueWithPersistentHash(
              `${histogramBase}.${moduleDescriptorId}`, id);
        }
      });
    }
  }

  /**
   * Manages the reloading of modules within the container based on
   * updates to the disabled modules list.
   *
   * Subsequent calls handle potential reloads. Newly enabled modules are
   * queued and loaded individually. The user does not see these modules until
   * the entire container is reloaded after all queued modules have been loaded.
   *
   * @param disabledModules - An object containing the current list of disabled
   * module ids.
   */
  private async handleModuleEnablement_(
      disabledModules: {all: boolean, ids: string[]}): Promise<void> {
    if (this.moduleLoadPromise_) {
      await this.moduleLoadPromise_;
    }

    if (!this.availableModulesIds_) {
      const modulesIdNames = (await this.pageHandler_.getModulesIdNames()).data;
      // TODO(crbug.com/385174675): Set |this.availableModulesIds_| in
      // |this.loadModules_()| when Microsoft modules are enabled ToT, as this
      // experimental behavior is currently gated to the Microsoft modules.
      this.availableModulesIds_ = new Set(modulesIdNames.map((m) => m.id));
    }

    const disabledModuleIds = disabledModules.ids;
    const newlyEnabledModuleIds = [...this.availableModulesIds_.difference(
        new Set(disabledModuleIds.concat(
            this.moduleInstances_.map((m) => m.descriptor.id))))];
    if (newlyEnabledModuleIds.length === 0) {
      return;
    }
    // Load modules one by one until the queue is empty.
    const newModuleInstances: ModuleInstance[] = [];
    while (newlyEnabledModuleIds.length > 0) {
      const moduleId = newlyEnabledModuleIds.shift()!;
      const module = await ModuleRegistry.getInstance().initializeModuleById(
          moduleId, loadTimeData.getInteger('modulesLoadTimeout'));
      if (module) {
        newModuleInstances.push(...createModuleInstances(module));
      }
    }

    if (newModuleInstances.length > 0) {
      newModuleInstances.push(...this.moduleInstances_);
      const orderedIds = (await this.pageHandler_.getModulesOrder()).moduleIds;
      if (orderedIds && orderedIds.length > 0) {
        newModuleInstances.sort((a, b) => {
          const aId = a.descriptor.id;
          const bId = b.descriptor.id;
          const aHasOrder = orderedIds.includes(aId);
          const bHasOrder = orderedIds.includes(bId);
          if (aHasOrder && bHasOrder) {
            return orderedIds.indexOf(aId) - orderedIds.indexOf(bId);
          }
          return +bHasOrder - +aHasOrder;
        });
      }

      this.moduleInstances_ = newModuleInstances;
      chrome.metricsPrivate.recordSmallCount(
          'NewTabPage.Modules.ReloadedModulesCount',
          this.moduleInstances_.length);
      this.recordModuleLoadedWithModules_(/*onNtpLoad=*/ false);
    }
  }

  private updateContainerAndChildrenStyles_(availableWidth: number) {
    const visibleModuleInstances = this.disabledModules_.all ?
        [] :
        this.moduleInstances_.filter(
            instance =>
                !this.disabledModules_.ids.includes(instance.descriptor.id));

    this.modulesShownToUser = visibleModuleInstances.length !== 0;
    if (visibleModuleInstances.length === 0) {
      return;
    }

    this.style.setProperty('--container-max-width', `${availableWidth}px`);

    const clamp = (min: number, val: number, max: number) =>
        Math.max(min, Math.min(val, max));
    const rowMaxInstanceCount = clamp(
        1,
        Math.floor(
            (availableWidth + CONTAINER_GAP_WIDTH) /
            (CONTAINER_GAP_WIDTH + SUPPORTED_MODULE_WIDTHS[0].value)),
        this.maxColumnCount_);

    let index = 0;
    while (index < visibleModuleInstances.length) {
      const instances =
          visibleModuleInstances.slice(index, index + rowMaxInstanceCount);
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

  protected onDisableModule_(e: DisableModuleEvent) {
    const id = ((e.target! as HTMLElement).parentNode as ModuleWrapperElement)
                   .module.descriptor.id;
    const restoreCallback = e.detail.restoreCallback;
    this.undoData_ = {
      message: e.detail.message,
      undo: () => {
        if (restoreCallback) {
          restoreCallback();
        }
        this.pageHandler_.setModuleDisabled(id, false);
        chrome.metricsPrivate.recordSparseValueWithPersistentHash(
            'NewTabPage.Modules.Enabled', id);
        chrome.metricsPrivate.recordSparseValueWithPersistentHash(
            'NewTabPage.Modules.Enabled.Toast', id);
      },
    };

    this.pageHandler_.setModuleDisabled(id, true);
    this.$.undoToast.show();
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        METRIC_NAME_MODULE_DISABLED, id);
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        `${METRIC_NAME_MODULE_DISABLED}.ModuleRequest`, id);
  }

  /**
   * @param e Event notifying a module instance was dismissed. Contains the
   *     message to show in the toast.
   */
  protected onDismissModuleInstance_(e: DismissModuleInstanceEvent) {
    const wrapper =
        ((e.target! as HTMLElement).parentNode as ModuleWrapperElement);
    const index = Array.from(wrapper.parentNode!.children).indexOf(wrapper);
    const module = this.moduleInstances_[index];
    this.moduleInstances_ = this.moduleInstances_.toSpliced(index, 1);

    const restoreCallback = e.detail.restoreCallback;
    this.undoData_ = {
      message: e.detail.message,
      undo: restoreCallback ?
          () => {
            this.moduleInstances_ =
                this.moduleInstances_.toSpliced(index, 0, module);
            restoreCallback();

            recordOccurrence('NewTabPage.Modules.Restored');
            recordOccurrence(
                `NewTabPage.Modules.Restored.${module.descriptor.id}`);
          } :
          undefined,
    };

    // Notify the user.
    this.$.undoToast.show();

    this.pageHandler_.onDismissModule(module.descriptor.id);
  }

  protected onDismissModuleElement_(e: DismissModuleElementEvent) {
    const restoreCallback = e.detail.restoreCallback;
    this.undoData_ = {
      message: e.detail.message,
      undo: restoreCallback ?
          () => {
            restoreCallback();
          } :
          undefined,
    };

    // Notify the user.
    this.$.undoToast.show();
  }

  protected onUndoButtonClick_() {
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

customElements.define(ModulesElement.is, ModulesElement);
