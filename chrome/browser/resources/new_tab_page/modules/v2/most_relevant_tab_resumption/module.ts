// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './page_favicon.js';
import '../icons.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit, loadTimeData} from '../../../i18n_setup.js';
import {recordOccurence as recordOccurrence} from '../../../metrics_utils.js';
import {ScoredURLUserAction} from '../../../most_relevant_tab_resumption.mojom-webui.js';
import type {URLVisit} from '../../../url_visit_types.mojom-webui.js';
import {FormFactor, VisitSource} from '../../../url_visit_types.mojom-webui.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getCss} from './module.css.js';
import {getHtml} from './module.html.js';
import {MostRelevantTabResumptionProxyImpl} from './most_relevant_tab_resumption_proxy.js';

export const MAX_URL_VISITS = 5;

export interface ModuleElement {
  $: {
    moduleHeaderElementV2: ModuleHeaderElement,
    urlVisits: HTMLElement,
  };
}

export class ModuleElement extends I18nMixinLit
(CrLitElement) {
  static get is() {
    return 'ntp-most-relevant-tab-resumption';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** The type of module width (wide, narrow, ...). */
      format: {
        type: String,
        reflect: true,
      },

      /** The cluster displayed by this element. */
      urlVisits: {type: Object},

      /**
       * To determine whether to show the module with the device icon.
       */
      shouldShowDeviceIcon_: {
        type: Boolean,
        reflect: true,
      },

      showInfoDialog_: {type: Boolean},
    };
  }

  format: string = 'wide';
  urlVisits: URLVisit[] = [];
  protected shouldShowDeviceIcon_: boolean =
    loadTimeData.getBoolean('mostRelevantTabResumptionDeviceIconEnabled');
  protected showInfoDialog_: boolean = false;

  protected getMenuItemGroups_(): MenuItem[][] {
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

  protected onDisableButtonClick_() {
    this.fire('disable-module', {
      message: loadTimeData.getStringF(
          'modulesDisableToastMessage',
          loadTimeData.getString('modulesThisTypeOfCardText')),
    });
  }

  protected onDismissAllButtonClick_() {
    MostRelevantTabResumptionProxyImpl.getInstance().handler.dismissModule(
        this.urlVisits);
    this.fire('dismiss-module-instance', {
      message: loadTimeData.getStringF(
          'dismissModuleToastMessage',
          loadTimeData.getString('modulesTabResumptionSentence')),
      restoreCallback: () => MostRelevantTabResumptionProxyImpl.getInstance()
                                 .handler.restoreModule(this.urlVisits),
    });
  }

  protected onSeeMoreButtonClick_() {
    this.fire('usage');
    recordOccurrence('NewTabPage.TabResumption.SeeMoreClick');
  }

  protected onDismissButtonClick_(e: Event) {
    e.preventDefault();   // Stop navigation
    e.stopPropagation();  // Stop firing of click handler
    const urlVisit = (e.target! as HTMLElement).parentElement!;
    const index = Number(urlVisit.dataset['index']);
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.TabResumption.VisitDismissIndex', index);
    urlVisit!.remove();
    MostRelevantTabResumptionProxyImpl.getInstance().handler.dismissURLVisit(
        this.urlVisits[index]);
    this.fire('dismiss-module-element', {
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
    });
  }

  protected onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }

  protected onUrlVisitClick_(e: Event) {
    this.fire('usage');
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    const urlVisit = this.urlVisits[index];
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.TabResumption.ClickIndex', index);
    chrome.metricsPrivate.recordEnumerationValue(
        'NewTabPage.TabResumption.Visit.ClickSource', urlVisit.source,
        VisitSource.MAX_VALUE);

    // Calculate the number of milliseconds in the difference. Max is 4 days.
    chrome.metricsPrivate.recordValue(
        {
          metricName: 'NewTabPage.TabResumption.TimeElapsedSinceLastVisit',
          type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG,
          min: 60 * 1000,
          max: 4 * 24 * 60 * 60 * 1000,
          buckets: 50,
        },
        Number(urlVisit.relativeTime.microseconds / 1000n));

    MostRelevantTabResumptionProxyImpl.getInstance().handler.recordAction(
        ScoredURLUserAction.kActivated, urlVisit.urlKey,
        urlVisit.trainingRequestId);
  }

  protected computeDomain_(urlVisit: URLVisit): string {
    let domain = (new URL(urlVisit.url.url)).hostname;
    domain = domain.replace('www.', '');
    return domain;
  }

  protected computeIcon_(urlVisit: URLVisit): string {
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

  protected computeDeviceName_(urlVisit: URLVisit): string|null {
    return loadTimeData.getBoolean('modulesRedesignedEnabled') ?
        urlVisit.sessionName :
        this.i18n('modulesTabResumptionDevicePrefix') +
            ` ${urlVisit.sessionName}`;
  }

  protected computeShouldShowDeviceName_(urlVisit: URLVisit): boolean {
    return !this.shouldShowDeviceIcon_ && !!this.computeDeviceName_(urlVisit);
  }

  protected getVisibleUrlVisits_(): URLVisit[] {
    return this.urlVisits.slice(0, MAX_URL_VISITS);
  }

  protected onInfoButtonClick_() {
    this.showInfoDialog_ = true;
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }
}

customElements.define(ModuleElement.is, ModuleElement);

async function createElement(): Promise<ModuleElement|null> {
  const {urlVisits} = await MostRelevantTabResumptionProxyImpl.getInstance()
                          .handler.getURLVisits();
  if (!urlVisits || urlVisits.length === 0) {
    return null;
  }

  const element = new ModuleElement();
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
