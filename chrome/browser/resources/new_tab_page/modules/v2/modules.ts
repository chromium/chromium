// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {NewTabPageProxy} from '../../new_tab_page_proxy.js';
import {WindowProxy} from '../../window_proxy.js';
import {ModuleRegistry} from '../module_registry.js';
import {ModuleInstance} from '../module_wrapper.js';

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
      moduleInstances_: {
        type: Array,
        value: [],
      },
    };
  }

  private mediaEventTracker_: EventTracker;
  private moduleInstances_: ModuleInstance[];

  constructor() {
    super();

    this.mediaEventTracker_ = new EventTracker();
  }

  override connectedCallback() {
    super.connectedCallback();

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
            thresholds[i] + (2 * MARGIN_WIDTH)}px) and (max-width: ${
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
      this.mediaEventTracker_.add(query, 'change', (e: MediaQueryListEvent) => {
        if (e.matches) {
          this.updateContainerAndChildrenStyles_(details.maxWidth);
        }
      });
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.mediaEventTracker_.removeAll();
  }

  override ready() {
    super.ready();

    this.updateStyles({
      '--container-gap': `${CONTAINER_GAP_WIDTH}px`,
    });

    this.loadModules_();
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
      this.moduleInstances_ = modules
                                  .map(module => {
                                    return module.elements.map(element => {
                                      return {
                                        element,
                                        descriptor: module.descriptor,
                                      };
                                    });
                                  })
                                  .flat();
      this.updateContainerAndChildrenStyles_(Math.min(
          document.body.clientWidth - 2 * MARGIN_WIDTH, CONTAINER_MAX_WIDTH));

      chrome.metricsPrivate.recordSmallCount(
          'NewTabPage.Modules.LoadedModulesCount', modules.length);
      // TODO(crbug.com/1444758): Add module instances count metric.
      this.dispatchEvent(new Event('modules-loaded'));
    }
  }

  private updateContainerAndChildrenStyles_(availableWidth: number) {
    if (this.moduleInstances_.length === 0) {
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
    while (index < this.moduleInstances_.length) {
      const instances =
          this.moduleInstances_.slice(index, index + rowMaxInstanceCount);
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
}

customElements.define(ModulesV2Element.is, ModulesV2Element);
