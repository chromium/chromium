// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './page_favicon.js';
import '../icons.html.js';

import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {recordOccurence as recordOccurrence} from '../../../metrics_utils.js';
import {ScoredURLUserAction} from '../../../most_relevant_tab_resumption.mojom-webui.js';
import type {URLVisit} from '../../../url_visit_types.mojom-webui.js';
import {FormFactor, VisitSource} from '../../../url_visit_types.mojom-webui.js';
import type {InfoDialogElement} from '../../info_dialog.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getTemplate} from './module.html.js';
import {MostRelevantTabResumptionProxyImpl} from './most_relevant_tab_resumption_proxy.js';

export const MAX_URL_VISITS = 5;

export interface MostRelevantTabResumptionModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
    moduleHeaderElementV2: ModuleHeaderElement,
    urlVisits: HTMLElement,
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
      urlVisits: {
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
urlVisits:
  URLVisit[];
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
        this.urlVisits);
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesTabResumptionSentence')),
        restoreCallback: () => MostRelevantTabResumptionProxyImpl.getInstance()
                                   .handler.restoreModule(this.urlVisits),
      },
    }));
  }

  private onSeeMoreButtonClick_() {
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
    recordOccurrence('NewTabPage.TabResumption.SeeMoreClick');
  }

  private onDismissButtonClick_(e: DomRepeatEvent<URLVisit>) {
    e.preventDefault();   // Stop navigation
    e.stopPropagation();  // Stop firing of click handler
    const urlVisit = (e.target! as HTMLElement).parentElement!;
    const index = e.model.index;
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.TabResumption.VisitDismissIndex', index);
    urlVisit!.remove();
    MostRelevantTabResumptionProxyImpl.getInstance().handler.dismissURLVisit(
        this.urlVisits[index]);
    this.dispatchEvent(new CustomEvent('dismiss-module-element', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesTabResumptionSentence')),
        restoreCallback: () => {
          chrome.metricsPrivate.recordSmallCount(
              'NewTabPage.TabResumption.VisitRestoreIndex', index);
          this.$.urlVisits.insertBefore(
              urlVisit, this.$.urlVisits.childNodes[index]);
          MostRelevantTabResumptionProxyImpl.getInstance()
              .handler.restoreURLVisit(this.urlVisits[index]);
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

  private onUrlVisitClick_(e: DomRepeatEvent<URLVisit>) {
    this.dispatchEvent(new Event('usage', {bubbles: true, composed: true}));
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.TabResumption.ClickIndex', e.model.index);
    chrome.metricsPrivate.recordEnumerationValue(
        'NewTabPage.TabResumption.Visit.ClickSource',
        this.urlVisits[e.model.index].source, VisitSource.MAX_VALUE);

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

    const urlVisit = this.urlVisits[e.model.index];
    MostRelevantTabResumptionProxyImpl.getInstance().handler.recordAction(
        ScoredURLUserAction.kActivated, urlVisit.urlKey,
        urlVisit.trainingRequestId);
  }

  private computeDomain_(urlVisit: URLVisit): string {
    let domain = (new URL(urlVisit.url.url)).hostname;
    domain = domain.replace('www.', '');
    return domain;
  }

  private computeIcon_(urlVisit: URLVisit): string {
    switch (urlVisit.formFactor) {
      case FormFactor.kDesktop:
        return 'tab_resumption:computer';
      case FormFactor.kPhone:
        return 'tab_resumption:phone';
      case FormFactor.kTablet:
        return 'tab_resumption:tablet';
      case FormFactor.kAutomotive:
        return 'tab_resumption:automotive';
      case FormFactor.kWearable:
        return 'tab_resumption:wearable';
      case FormFactor.kTv:
        return 'tab_resumption:tv';
      default:
        return 'tab_resumption:globe';
    }
  }

  private computeDeviceName_(urlVisit: URLVisit): string|null {
    return loadTimeData.getBoolean('modulesRedesignedEnabled') ?
        urlVisit.sessionName :
        this.i18n('modulesTabResumptionDevicePrefix') +
            ` ${urlVisit.sessionName}`;
  }

  private computeFaviconSize_(): number {
    return 24;
  }
  private computeShouldShowDeviceName_(urlVisit: URLVisit): boolean {
    return !this.shouldShowDeviceIcon_ && !!this.computeDeviceName_(urlVisit);
  }

  private getVisibleUrlVisits_(): URLVisit[] {
    return this.urlVisits.slice(0, MAX_URL_VISITS);
  }
}

customElements.define(
    MostRelevantTabResumptionModuleElement.is,
    MostRelevantTabResumptionModuleElement);

async function createElement():
    Promise<MostRelevantTabResumptionModuleElement|null> {
  const {urlVisits} = await MostRelevantTabResumptionProxyImpl.getInstance()
                          .handler.getURLVisits();
  if (!urlVisits || urlVisits.length === 0) {
    return null;
  }

  const element = new MostRelevantTabResumptionModuleElement();
  element.urlVisits = urlVisits;

  urlVisits.slice(0, MAX_URL_VISITS).forEach((urlVisit) => {
    MostRelevantTabResumptionProxyImpl.getInstance().handler.recordAction(
        ScoredURLUserAction.kSeen, urlVisit.urlKey, urlVisit.trainingRequestId);
  });

  return element;
}

export const mostRelevantTabResumptionDescriptor: ModuleDescriptor =
    new ModuleDescriptor(
        /*id=*/ 'tab_resumption', createElement);
