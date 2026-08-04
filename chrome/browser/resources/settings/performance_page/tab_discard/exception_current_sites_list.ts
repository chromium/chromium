// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_scrollable.css.js';
import '../../settings_shared.css.js';
import '../../site_favicon.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {convertDateToWindowsEpoch} from '../../time.js';
import type {PerformanceBrowserProxy} from '../performance_browser_proxy.js';
import {PerformanceBrowserProxyImpl} from '../performance_browser_proxy.js';
import type {PerformanceMetricsProxy} from '../performance_metrics_proxy.js';
import {MemorySaverModeExceptionListAction, PerformanceMetricsProxyImpl} from '../performance_metrics_proxy.js';

import {getTemplate} from './exception_current_sites_list.html.js';
import {TAB_DISCARD_EXCEPTIONS_PREF} from './exception_validation_mixin.js';

export interface ExceptionCurrentSitesListElement {
  $: {
    list: HTMLElement,
  };
}

type Site = string;

export class ExceptionCurrentSitesListElement extends PolymerElement {
  static get is() {
    return 'tab-discard-exception-current-sites-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      currentSites_: {type: Array, value: []},

      selectedSites_: {
        type: Array,
        value() {
          return new Set();
        },
      },

      submitDisabled: {
        type: Boolean,
        notify: true,
      },

      updateIntervalMS_: {
        type: Number,
        value: 1000,
      },

      // whether the current sites list is visible according to its parent
      visible: {
        type: Boolean,
        value: true,
        observer: 'onVisibilityChanged_',
      },
    };
  }

  private browserProxy_: PerformanceBrowserProxy =
      PerformanceBrowserProxyImpl.getInstance();
  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  declare private currentSites_: Site[];
  declare private selectedSites_: Set<Site>;
  declare private submitDisabled: boolean;
  declare private updateIntervalMS_: number;
  declare visible: boolean;

  private onVisibilityChangedListener_: () => void;
  private updateIntervalID_: number|undefined = undefined;

  override async connectedCallback() {
    super.connectedCallback();

    await this.updateCurrentSites_();
    this.dispatchEvent(new CustomEvent('sites-populated', {
      detail: {length: this.currentSites_.length},
    }));

    // TODO(crbug.com/540914692): Workaround for Blink bug, by resetting
    // focusgroup attribute restores FocusgroupData that was wiped out during
    // detachment/attachment. Can probably remove this after migrating to Lit,
    // since detachment/attachment happens due to the parent chain using dom-if.
    this.$.list.setAttribute(
        'focusgroup', this.$.list.getAttribute('focusgroup')!);

    this.onVisibilityChanged_();
    this.onVisibilityChangedListener_ = this.onVisibilityChanged_.bind(this);
    document.addEventListener(
        'visibilitychange', this.onVisibilityChangedListener_);
  }

  override disconnectedCallback() {
    document.removeEventListener(
        'visibilitychange', this.onVisibilityChangedListener_);
    this.stopUpdatingCurrentSites_();
  }

  private onVisibilityChanged_() {
    if (this.visible && document.visibilityState === 'visible') {
      this.startUpdatingCurrentSites_();
    } else {
      this.stopUpdatingCurrentSites_();
    }
  }

  private startUpdatingCurrentSites_() {
    this.updateCurrentSites_().then(() => {
      if (this.updateIntervalID_ === undefined) {
        this.updateIntervalID_ = setInterval(
            this.updateCurrentSites_.bind(this), this.updateIntervalMS_);
      }
    });
  }

  private stopUpdatingCurrentSites_() {
    if (this.updateIntervalID_ !== undefined) {
      clearInterval(this.updateIntervalID_);
      this.updateIntervalID_ = undefined;
    }
  }

  setUpdateIntervalForTesting(updateIntervalMS: number) {
    this.updateIntervalMS_ = updateIntervalMS;
    this.stopUpdatingCurrentSites_();
    this.startUpdatingCurrentSites_();
  }

  getIsUpdatingForTesting() {
    return this.updateIntervalID_ !== undefined;
  }

  private async updateCurrentSites_() {
    await PrefService.getInstance().whenInitialized();
    const existingSites = new Set(Object.keys(
        PrefService.getInstance()
            .getPref<Record<string, unknown>>(TAB_DISCARD_EXCEPTIONS_PREF)
            .value));
    const currentSites = (await this.browserProxy_.getCurrentOpenSites())
                             .filter(rule => !existingSites.has(rule));

    // Remove sites from selected set that are no longer in the list.
    this.selectedSites_ =
        new Set(currentSites.filter(this.isSelectedSite_.bind(this)));
    this.computeSubmitDisabled_();

    this.currentSites_ = currentSites;
  }

  private computeSubmitDisabled_() {
    this.submitDisabled = !this.selectedSites_.size;
  }


  // Called to recalculate checked status of entries when the site changes due
  // to list updates.
  private isSelectedSite_(site: Site) {
    return this.selectedSites_.has(site);
  }

  private onToggleSelection_(e: DomRepeatEvent<Site>) {
    const checkbox = e.target as CrCheckboxElement;
    if (checkbox.checked) {
      this.selectedSites_.add(e.model.item);
    } else {
      this.selectedSites_.delete(e.model.item);
    }
    this.computeSubmitDisabled_();
  }

  submit() {
    assert(!this.submitDisabled);
    const epoch = convertDateToWindowsEpoch();
    this.selectedSites_.forEach(rule => {
      PrefService.getInstance().setPrefDictEntry(
          TAB_DISCARD_EXCEPTIONS_PREF, rule, epoch);
    });
    this.metricsProxy_.recordExceptionListAction(
        MemorySaverModeExceptionListAction.ADD_FROM_CURRENT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-current-sites-list':
        ExceptionCurrentSitesListElement;
  }
}

customElements.define(
    ExceptionCurrentSitesListElement.is,
    ExceptionCurrentSitesListElement);
