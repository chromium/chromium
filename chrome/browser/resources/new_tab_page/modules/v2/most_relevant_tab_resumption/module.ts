// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../tab_resumption/page_favicon.js';
import '../icons.html.js';

import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Tab} from '../../../history_types.mojom-webui.js';
import {DeviceType} from '../../../history_types.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {ScoredURLUserAction} from '../../../most_relevant_tab_resumption.mojom-webui.js';
import type {InfoDialogElement} from '../../info_dialog.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getTemplate} from './module.html.js';
import {MostRelevantTabResumptionProxyImpl} from './most_relevant_tab_resumption_proxy.js';

export const MAX_TABS = 5;

export interface MostRelevantTabResumptionModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
    moduleHeaderElementV2: ModuleHeaderElement,
    tabs: HTMLElement,
  };
}

export class MostRelevantTabResumptionModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-most-relevant-tab-resumption';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The type of module width (wide, narrow, ...). */
      format: {
        type: String,
        reflectToAttribute: true,
      },

      /** The cluster displayed by this element. */
      tabs: {
        type: Object,
      },

      /**
       * To determine whether to show the module with the device icon.
       */
      shouldShowDeviceIcon_: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean(
            'mostRelevantTabResumptionDeviceIconEnabled'),
      },
    };
  }

format:
  string;
tabs:
  Tab[];
private shouldShowDeviceIcon_:
  boolean;

  private getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:thumb_down',
          text: this.i18n('modulesMostRelevantTabResumptionDismissAll'),
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

  private onDismissAllButtonClick_() {
    MostRelevantTabResumptionProxyImpl.getInstance().handler.dismissModule(
        this.tabs);
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesTabResumptionSentence')),
        restoreCallback: () => MostRelevantTabResumptionProxyImpl.getInstance()
                                   .handler.restoreModule(this.tabs),
      },
    }));
  }

  private onDismissButtonClick_(e: DomRepeatEvent<Tab>) {
    e.preventDefault();
    const tab = (e.target! as HTMLElement).parentElement!;
    const index = e.model.index;
    tab!.remove();
    MostRelevantTabResumptionProxyImpl.getInstance().handler.dismissTab(
        this.tabs[index]);
    this.dispatchEvent(new CustomEvent('dismiss-module-element', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesTabResumptionSentence')),
        restoreCallback: () => {
          this.$.tabs.insertBefore(tab, this.$.tabs.childNodes[index]);
          MostRelevantTabResumptionProxyImpl.getInstance().handler.restoreTab(
              this.tabs[index]);
        },
      },
    }));
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

    const tab = this.tabs[e.model.index];
    MostRelevantTabResumptionProxyImpl.getInstance().handler.recordAction(
        ScoredURLUserAction.kActivated, tab.urlKey, tab.trainingRequestId);
  }

  private computeDomain_(tab: Tab): string {
    let domain = (new URL(tab.url.url)).hostname;
    domain = domain.replace('www.', '');
    return domain;
  }

  private computeIcon_(tab: Tab): string {
    switch (tab.deviceType) {
      case DeviceType.kDesktop:
        return 'tab_resumption:computer';
      case DeviceType.kPhone:
        return 'tab_resumption:phone';
      case DeviceType.kTablet:
        return 'tab_resumption:tablet';
      default:
        return 'tab_resumption:globe';
    }
  }

  private computeDeviceName_(tab: Tab): string|null {
    return loadTimeData.getBoolean('modulesRedesignedEnabled') ?
        tab.sessionName :
        this.i18n('modulesTabResumptionDevicePrefix') + ` ${tab.sessionName}`;
  }

  private computeFaviconSize_(): number {
    return 24;
  }

  private computeShouldShowDeviceName_(tab: Tab): boolean {
    return !this.shouldShowDeviceIcon_ && !!this.computeDeviceName_(tab);
  }

  private getVisibleTabs_(): Tab[] {
    return this.tabs.slice(0, MAX_TABS);
  }
}

customElements.define(
    MostRelevantTabResumptionModuleElement.is,
    MostRelevantTabResumptionModuleElement);

async function createElement():
    Promise<MostRelevantTabResumptionModuleElement|null> {
  const {tabs} =
      await MostRelevantTabResumptionProxyImpl.getInstance().handler.getTabs();
  if (!tabs || tabs.length === 0) {
    return null;
  }

  const element = new MostRelevantTabResumptionModuleElement();
  element.tabs = tabs;

  tabs.slice(0, MAX_TABS).forEach((tab) => {
    MostRelevantTabResumptionProxyImpl.getInstance().handler.recordAction(
        ScoredURLUserAction.kSeen, tab.urlKey, tab.trainingRequestId);
  });

  return element;
}

export const mostRelevantTabResumptionDescriptor: ModuleDescriptor =
    new ModuleDescriptor(
        /*id=*/ 'tab_resumption', createElement);
