// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import type {HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import type {TemplateInstanceBase} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement, templatize} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {recordOccurence as recordOccurrence} from '../../metrics_utils.js';
import type {PageCallbackRouter, PageHandlerRemote} from '../../new_tab_page.mojom-webui.js';
import {IphFeature} from '../../new_tab_page.mojom-webui.js';
import type {ModuleIdName} from '../../new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from '../../new_tab_page_proxy.js';
import {WindowProxy} from '../../window_proxy.js';
import type {Module, ModuleDescriptor} from '../module_descriptor.js';
import {ModuleRegistry} from '../module_registry.js';
import type {ModuleInstance, ModuleWrapperElement} from '../module_wrapper.js';

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

type ModuleWrapperConstructor = new (_: Object) =>
    TemplateInstanceBase&HTMLElement;

interface ItemTemplateInstance extends TemplateInstanceBase {
  item: {descriptor: ModuleDescriptor};
}

declare global {
  interface HTMLElementEventMap {
    'disable-module': DisableModuleEvent;
    'dismiss-module-instance': DismissModuleInstanceEvent;
    'dismiss-module-element': DismissModuleElementEvent;
  }
}

export interface ModulesV2Element {
  $: {
    container: HTMLElement,
    undoToast: CrToastElement,
    undoToastMessage: HTMLElement,
  };
}

export const MODULE_CUSTOMIZE_ELEMENT_ID =
    'NewTabPageUI::kModulesCustomizeIPHAnchorElement';

/**
 * Computes the IDs of modules that have been newly enabled.
 *
 * This function compares the current and previous lists of disabled module IDs
 * to identify modules that are now enabled. It then filters out modules that
 * already have template instances, ensuring that only truly newly enabled
 * modules are returned.
 *
 * @param newDisabledIds The latest list of disabled module IDs.
 * @param prevDisabledIds The previous list of disabled module IDs.
 * @param templateInstances A list of existing template instances.
 * @returns An array of module IDs that are newly enabled and do not yet have
 *     template instances.
 */
function computeNewlyEnabledModuleIds(
    newDisabledIds: string[], prevDisabledIds: string[],
    templateInstances: TemplateInstanceBase[]): string[] {
  const enabledIds = prevDisabledIds.filter(id => !newDisabledIds.includes(id));
  const newlyEnabledModuleIds = enabledIds.filter((enabledId) => {
    return !templateInstances.some(
        (templateInstance) =>
            (templateInstance as unknown as ItemTemplateInstance)
                .item.descriptor.id === enabledId);
  });
  return newlyEnabledModuleIds;
}

/**
 * Creates template instances for a list of modules.
 *
 * @param modules The modules for which to create template instances.
 * @param moduleWrapperConstructor The constructor used to create the template
 *     instances.
 * @returns An array of `TemplateInstanceBase` objects.
 */
function createTemplateInstances(
    modules: Module[], moduleWrapperConstructor: ModuleWrapperConstructor):
    TemplateInstanceBase[] {
  return modules.flatMap(module => module.elements.map(element => {
    const instanceData = {
      element,
      descriptor: module.descriptor,
    };
    return new moduleWrapperConstructor({item: instanceData});
  }));
}

const AppElementBase = HelpBubbleMixin(PolymerElement) as
    {new (): PolymerElement & HelpBubbleMixinInterface};

/** Container for the NTP modules. */
export class ModulesV2Element extends AppElementBase {
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

      modulesShownToUser: {
        type: Boolean,
        notify: true,
      },

      /** Data about the most recent un-doable action. */
      undoData_: {
        type: Object,
        value: null,
      },
    };
  }

  modulesShownToUser: boolean;
  private waitToLoadModules_: boolean =
      loadTimeData.getBoolean('waitToLoadModules');
  private maxColumnCount_: number;
  private containerMaxWidth_: number;
  private disabledModules_: {all: boolean, ids: string[]};
  private eventTracker_: EventTracker = new EventTracker();
  private undoData_: {message: string, undo?: () => void}|null;
  private setDisabledModulesListenerId_: number|null = null;
  private setModulesLoadableListenerId_: number|null = null;
  private containerObserver_: MutationObserver|null = null;
  private templateInstances_: TemplateInstanceBase[] = [];
  private modulesLoaded_: boolean = false;
  // TODO(crbug.com/385174675): Remove |modulesReloadable_| flag when safe.
  // Otherwise, when Microsoft modules are enabled ToT, the current behavior
  // gated by |modulesReloadable_| should become the default module loading
  // behavior.
  private modulesReloadable_: boolean =
      loadTimeData.getBoolean('modulesReloadable');
  private moduleWrapperConstructor_: ModuleWrapperConstructor|null = null;
  private newlyEnabledModuleIds_: string[] = [];

  private callbackRouter_: PageCallbackRouter;
  private handler_: PageHandlerRemote;
  private moduleRegistry_: ModuleRegistry;

  constructor() {
    super();
    this.callbackRouter_ = NewTabPageProxy.getInstance().callbackRouter;
    this.handler_ = NewTabPageProxy.getInstance().handler;
    this.moduleRegistry_ = ModuleRegistry.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.setDisabledModulesListenerId_ =
        this.callbackRouter_.setDisabledModules.addListener(
            (all: boolean, ids: string[]) => {
              if (!this.modulesReloadable_) {
                this.disabledModules_ = {all, ids};
              } else {
                this.handleModuleEnablement_({all, ids});
              }
            });
    this.handler_.updateDisabledModules();

    this.setModulesLoadableListenerId_ =
        this.callbackRouter_.setModulesLoadable.addListener(() => {
          if (this.waitToLoadModules_) {
            this.waitToLoadModules_ = false;
            this.loadModules_();
          }
        });

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

    this.containerObserver_ = new MutationObserver(() => {
      this.updateContainerAndChildrenStyles_();
    });
    this.containerObserver_!.observe(this.$.container, {childList: true});
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.setDisabledModulesListenerId_);
    this.callbackRouter_.removeListener(this.setDisabledModulesListenerId_);
    assert(this.setModulesLoadableListenerId_);
    this.callbackRouter_.removeListener(this.setModulesLoadableListenerId_);

    this.eventTracker_.removeAll();

    this.containerObserver_!.disconnect();
  }

  override ready() {
    super.ready();

    this.updateStyles({
      '--container-gap': `${CONTAINER_GAP_WIDTH}px`,
    });

    this.maxColumnCount_ = loadTimeData.getInteger('modulesMaxColumnCount');
    this.containerMaxWidth_ =
        this.maxColumnCount_ * SUPPORTED_MODULE_WIDTHS[0].value +
        (this.maxColumnCount_ - 1) * CONTAINER_GAP_WIDTH;

    if (this.waitToLoadModules_) {
      this.handler_.updateModulesLoadable();
    } else {
      this.loadModules_();
    }
  }

  private moduleDisabled_(
      disabledModules: {all: true, ids: string[]},
      instance: ModuleInstance): boolean {
    return disabledModules.all ||
        disabledModules.ids.includes(instance.descriptor.id);
  }

  /**
   * Manages the loading and reloading of modules within the container based on
   * updates to the disabled modules list.
   *
   * If no modules have been shown to the user yet, |loadModules_()| is
   * called to populate the container.
   *
   * Subsequent calls handle potential reloads. Newly enabled modules are
   * queued and loaded individually. The user does not see these modules until
   * the entire container is reloaded via |reloadModules_()| after all
   * queued modules have been loaded. Loading continues as long as
   * |this.newlyEnabledModuleIds_| is not empty, even across multiple calls to
   * |maybeLoadModules_()|.
   *
   * @param newDisabledModules - Object specifying which modules are disabled.
   *    `all: If true, all modules are disabled.
   *    `ids: List of disabled module IDs.
   */
  private async handleModuleEnablement_(
      newDisabledModules: {all: boolean, ids: string[]}): Promise<void> {
    if (!this.modulesLoaded_ || this.templateInstances_.length === 0) {
      // Update the disabled modules list before attempting load.
      // |loadModules_()| expects the disabled module list to be
      // up-to-date.
      this.disabledModules_ = newDisabledModules;
      this.loadModules_();
      return;
    }

    const prevDisabledIds = this.disabledModules_.ids;
    const newlyEnabledModuleIds = computeNewlyEnabledModuleIds(
        newDisabledModules.ids, prevDisabledIds, this.templateInstances_);
    this.newlyEnabledModuleIds_ =
        this.newlyEnabledModuleIds_.concat(newlyEnabledModuleIds);

    if (this.newlyEnabledModuleIds_.length === 0) {
      this.disabledModules_ = newDisabledModules;
      return;
    }

    // Load modules one by one until the queue is empty.
    while (this.newlyEnabledModuleIds_.length > 0) {
      await this.addTemplateInstance_(this.newlyEnabledModuleIds_[0]);
      this.newlyEnabledModuleIds_.shift();
      if (this.newlyEnabledModuleIds_.length === 0) {
        this.disabledModules_ = newDisabledModules;
        await this.reloadModules_();
      } else {
        // More modules to load; the next call to this function will continue
        // the process.
        return;
      }
    }
  }

  /**
   * Initializes the module container by loading all currently enabled modules.
   * This method uses |this.moduleRegistry_| to determine which modules to load
   * and is called only when the container is empty.
   */
  private async loadModules_(): Promise<void> {
    if (this.waitToLoadModules_) {
      return;
    }

    const modulesIdNames = (await this.handler_.getModulesIdNames()).data;
    const modules = await this.moduleRegistry_.initializeModulesHavingIds(
        modulesIdNames.map((m: ModuleIdName) => m.id),
        loadTimeData.getInteger('modulesLoadTimeout'));
    if (modules) {
      this.handler_.onModulesLoadedWithData(
          modules.map(module => module.descriptor.id));

      // TODO(crbug.com/392889804): Remove this logic, since no modules populate
      // more than once anymore.
      if (modules.length > 1) {
        const maxModuleInstanceCount =
            (modules.length >= this.maxColumnCount_) ?
            1 :
            loadTimeData.getInteger(
                'multipleLoadedModulesMaxModuleInstanceCount');
        if (maxModuleInstanceCount > 0) {
          modules.forEach(module => {
            module.elements.splice(
                maxModuleInstanceCount,
                module.elements.length - maxModuleInstanceCount);
          });
        }
      }

      if (modules.length > 0) {
        if (!this.moduleWrapperConstructor_) {
          this.initModuleWrapperConstructor_();
        }

        this.templateInstances_ =
            createTemplateInstances(modules, this.moduleWrapperConstructor_!);
        this.$.container.replaceChildren(
            ...this.templateInstances_.map(t => t.children[0] as HTMLElement));
      }

      if (!this.modulesLoaded_) {
        this.onInitialLoadDone_(modules, modulesIdNames);
      }

      if (this.templateInstances_.length > 0) {
        this.registerHelpBubble(
            MODULE_CUSTOMIZE_ELEMENT_ID,
            [
              '#container',
              'ntp-module-wrapper',
              '#moduleElement',
            ],
            {fixed: true});
        // TODO(crbug.com/40075330): Currently, a period of time must elapse
        // between the registration of the anchor element and the promo
        // invocation, else the anchor element will not be ready for use.
        setTimeout(() => {
          this.handler_.maybeShowFeaturePromo(IphFeature.kCustomizeModules);
        }, 1000);
      }
    }
  }

  private initModuleWrapperConstructor_() {
    const template = this.shadowRoot!.querySelector('template')!;
    this.moduleWrapperConstructor_ = templatize(template, this, {
                                       parentModel: true,
                                       forwardHostProp: this.forwardHostProp_,
                                       instanceProps: {item: true},
                                     }) as new (item: Object) =>
                                         TemplateInstanceBase & HTMLElement;
  }

  /**
   * Records initial load metrics and dispatches the `modules-loaded` event.
   * @param modules The list of loaded `Module` objects.
   * @param modulesIdNames The list of `ModuleIdName` objects, each containing
   *     the ID and name of an available NTP module (regardless of
   *     enabled/disabled state).
   */
  private onInitialLoadDone_(
      modules: Module[], modulesIdNames: ModuleIdName[]) {
    this.modulesLoaded_ = true;

    // TODO(crbug.com/392707563): Decide if any of these metrics should be
    // recorded again whenever the module container is reloaded.
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.Modules.LoadedModulesCount', modules.length);
    modulesIdNames.forEach(({id}) => {
      chrome.metricsPrivate.recordBoolean(
          `NewTabPage.Modules.EnabledOnNTPLoad.${id}`,
          !this.disabledModules_.all &&
              !this.disabledModules_.ids.includes(id));
    });
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.Modules.InstanceCount', this.templateInstances_.length);
    chrome.metricsPrivate.recordBoolean(
        'NewTabPage.Modules.VisibleOnNTPLoad', !this.disabledModules_.all);
    this.recordModuleLoadedWithModules_(modules);

    this.dispatchEvent(new Event('modules-loaded'));
  }

  private recordModuleLoadedWithModules_(modules: Module[]) {
    const moduleDescriptorIds = modules.map(m => m.descriptor.id);

    for (const moduleDescriptorId of moduleDescriptorIds) {
      moduleDescriptorIds.forEach(id => {
        if (id !== moduleDescriptorId) {
          chrome.metricsPrivate.recordSparseValueWithPersistentHash(
              `NewTabPage.Modules.LoadedWith.${moduleDescriptorId}`, id);
        }
      });
    }
  }

  /**
   * Creates a template instance for a module then appends it to
   * |this.templateInstances_| based on the provided module id.
   *
   * @param moduleId A module id to be leveraged when determining the
   *     module to be initialized.
   */
  private async addTemplateInstance_(moduleId: string): Promise<void> {
    const module = await this.moduleRegistry_.initializeModuleById(
        moduleId, loadTimeData.getInteger('modulesLoadTimeout'));

    if (!module) {
      return;
    }

    if (!this.moduleWrapperConstructor_) {
      this.initModuleWrapperConstructor_();
    }

    this.templateInstances_ = this.templateInstances_.concat(
        ...createTemplateInstances([module], this.moduleWrapperConstructor_!));
  }

  /**
   * Reloads the modules container by sorting template instances based on the
   * order provided by |this.handler_| and then updating the container's DOM.
   */
  private async reloadModules_(): Promise<void> {
    const orderedIds = (await this.handler_.getModulesOrder()).moduleIds;
    this.templateInstances_ = this.templateInstances_.sort((a, b) => {
      const aIndex = orderedIds.indexOf(
          (a as unknown as ItemTemplateInstance).item.descriptor.id);
      const bIndex = orderedIds.indexOf(
          (b as unknown as ItemTemplateInstance).item.descriptor.id);
      return aIndex - bIndex;
    });

    this.$.container.replaceChildren(
        ...this.templateInstances_.map(t => t.children[0] as HTMLElement));
  }

  private forwardHostProp_(property: string, value: any) {
    this.templateInstances_.forEach(instance => {
      instance.forwardHostProp(property, value);
    });
  }

  private updateContainerAndChildrenStyles_(availableWidth?: number) {
    if (typeof availableWidth === 'undefined') {
      availableWidth = Math.min(
          document.body.clientWidth - 2 * MARGIN_WIDTH,
          this.containerMaxWidth_);
    }

    const moduleWrappers =
        Array.from(this.shadowRoot!.querySelectorAll(
            'ntp-module-wrapper:not([hidden])')) as ModuleWrapperElement[];
    this.modulesShownToUser = moduleWrappers.length !== 0;
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
        this.maxColumnCount_);

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
        this.handler_.setModuleDisabled(id, false);
        chrome.metricsPrivate.recordSparseValueWithPersistentHash(
            'NewTabPage.Modules.Enabled', id);
        chrome.metricsPrivate.recordSparseValueWithPersistentHash(
            'NewTabPage.Modules.Enabled.Toast', id);
      },
    };

    this.handler_.setModuleDisabled(id, true);
    this.$.undoToast.show();
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        METRIC_NAME_MODULE_DISABLED, id);
    chrome.metricsPrivate.recordSparseValueWithPersistentHash(
        `${METRIC_NAME_MODULE_DISABLED}.ModuleRequest`, id);
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

            recordOccurrence('NewTabPage.Modules.Restored');
            recordOccurrence(
                `NewTabPage.Modules.Restored.${wrapper.module.descriptor.id}`);
          } :
          undefined,
    };

    // Notify the user.
    this.$.undoToast.show();

    this.handler_.onDismissModule(wrapper.module.descriptor.id);
  }

  private onDismissModuleElement_(e: DismissModuleElementEvent) {
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
