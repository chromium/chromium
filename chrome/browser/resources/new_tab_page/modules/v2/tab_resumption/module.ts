// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../history_clusters/page_favicon.js';

import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Tab} from '../../../history_types.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import type {InfoDialogElement} from '../../info_dialog.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElementV2} from '../module_header.js';

import {getTemplate} from './module.html.js';
import {TabResumptionProxyImpl} from './tab_resumption_proxy.js';

export const MAX_TABS =
    !loadTimeData.getBoolean('modulesRedesignedEnabled') ? 3 : 5;

export interface TabResumptionModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
    moduleHeaderElementV2: ModuleHeaderElementV2,
  };
}

export class TabResumptionModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-tab-resumption';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The cluster displayed by this element. */
      tabs: {
        type: Object,
      },

      /** To determine if the hover layer should have all rounded corners. */
      isSingleTab_: {
        type: Boolean,
        reflectToAttribute: true,
        computed: `computeIsSingleTab_(tabs)`,
      },

      /**
       * Although this is a V2 class, we use this to make it work for V1
       * modules.
       */
      modulesRedesigned_: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean('modulesRedesignedEnabled'),
      },
    };
  }

tabs:
  Tab[];

  private getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:thumb_down',
          text: this.i18n('modulesTabResumptionDismissButton'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18nRecursive(
              '', 'modulesDisableButtonTextV2', 'modulesTabResumptionTitle'),
        },
        {
          action: 'info',
          icon: 'modules:info',
          text: this.i18n('moduleInfoButtonTitle'),
        },
      ],
      [
        {
          action: 'customize-module',
          icon: 'modules:tune',
          text: this.i18n('modulesCustomizeButtonText'),
        },
      ],
    ];
  }

  private onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'modulesDisableToastMessage',
            loadTimeData.getString('modulesThisTypeOfCardText')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }

  private onTabClick_(e: DomRepeatEvent<Tab>) {
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.TabResumption.ClickIndex', e.model.index);

    // Calculate the number of milliseconds in the difference. Max is 4 days.
    chrome.metricsPrivate.recordValue(
        {
          metricName: 'NewTabPage.TabResumption.TimeElapsedSinceLastVisit',
          type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
          min: 60 * 1000,
          max: 4 * 24 * 60 * 60 * 1000,
          buckets: 50,
        },
        Number(e.model.item.relativeTime.microseconds / 1000n));
  }

  private onDismissButtonClick_() {
    const urls = this.tabs.map((tab: Tab) => tab.url);
    TabResumptionProxyImpl.getInstance().handler.dismissModule(urls);
    this.dispatchEvent(new CustomEvent(
        loadTimeData.getBoolean('modulesRedesignedEnabled') ?
            'dismiss-module-instance' :
            'dismiss-module',
        {
          bubbles: true,
          composed: true,
          detail: {
            message: loadTimeData.getStringF(
                'dismissModuleToastMessage',
                loadTimeData.getString('modulesTabResumptionSentence')),
            restoreCallback: () =>
                TabResumptionProxyImpl.getInstance().handler.restoreModule(),
          },
        }));
  }

  private computeDomain_(tab: Tab): string {
    let domain = (new URL(tab.url.url)).hostname;
    domain = domain.replace('www.', '');
    return domain;
  }

  private computeDeviceName_(tab: Tab): string|null {
    return loadTimeData.getBoolean('modulesRedesignedEnabled') ?
        tab.sessionName :
        this.i18n('modulesTabResumptionDevicePrefix') + ` ${tab.sessionName}`;
  }

  private computeIsSingleTab_(): boolean {
    return this.tabs && this.tabs.length === 1;
  }

  private computeFaviconSize_(): number {
    return loadTimeData.getBoolean('modulesRedesignedEnabled') ? 18 : 19;
  }
}

customElements.define(
    TabResumptionModuleElement.is, TabResumptionModuleElement);

async function createElement(): Promise<TabResumptionModuleElement|null> {
  const {tabs} = await TabResumptionProxyImpl.getInstance().handler.getTabs();
  if (!tabs || tabs.length === 0) {
    return null;
  }

  const element = new TabResumptionModuleElement();
  element.tabs = tabs.slice(0, MAX_TABS);
  chrome.metricsPrivate.recordSmallCount('NewTabPage.TabResumption.TabCount',
    element.tabs.length);

  return element;
}

export const tabResumptionDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'tab_resumption', createElement);
