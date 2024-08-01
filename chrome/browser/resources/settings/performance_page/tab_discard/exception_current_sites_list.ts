// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../../controls/settings_checkbox_list_entry.js';
import '../../settings_shared.css.js';
import '../../site_favicon.js';

import type {PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {ListPropertyUpdateMixinInterface} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ScrollableMixinInterface} from '../../scrollable_mixin.js';
import {ScrollableMixin} from '../../scrollable_mixin.js';
import {convertDateToWindowsEpoch} from '../../time.js';
import type {PerformanceBrowserProxy} from '../performance_browser_proxy.js';
import {PerformanceBrowserProxyImpl} from '../performance_browser_proxy.js';
import type {PerformanceMetricsProxy} from '../performance_metrics_proxy.js';
import {MemorySaverModeExceptionListAction, PerformanceMetricsProxyImpl} from '../performance_metrics_proxy.js';

import {getTemplate} from './exception_current_sites_list.html.js';
import {TAB_DISCARD_EXCEPTIONS_PREF} from './exception_validation_mixin.js';

export interface ExceptionCurrentSitesListElement {
  $: {
    list: IronListElement,
  };
}

type Site = string;

type Constructor<T> = new (...args: any[]) => T;
const ExceptionCurrentSitesListElementBase =
    ListPropertyUpdateMixin(ScrollableMixin(PrefsMixin(PolymerElement))) as
    Constructor<ListPropertyUpdateMixinInterface&ScrollableMixinInterface&
                PrefsMixinInterface&PolymerElement>;

export class ExceptionCurrentSitesListElement extends
    ExceptionCurrentSitesListElementBase {
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

  private currentSites_: Site[];
  private selectedSites_: Set<Site>;
  private submitDisabled: boolean;
  private updateIntervalMS_: number;
  visible: boolean;

  private onVisibilityChangedListener_: () => void;
  private updateIntervalID_: number|undefined = undefined;

  override async connectedCallback() {
    super.connectedCallback();

    await this.updateCurrentSites_();
    this.dispatchEvent(new CustomEvent('sites-populated', {
      detail: {length: this.currentSites_.length},
    }));

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

  // Notifies the iron-list child that it should resize (generally because this
  // element's visibility has changed).
  notifyResize() {
    this.$.list.notifyResize();
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
    const existingSites =
        new Set(Object.keys(this.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value));
    const currentSites = (await this.browserProxy_.getCurrentOpenSites())
                             .filter(rule => !existingSites.has(rule));

    // Remove sites from selected set that are no longer in the list.
    this.selectedSites_ =
        new Set(currentSites.filter(this.isSelectedSite_.bind(this)));
    this.computeSubmitDisabled_();

    this.updateList('currentSites_', x => x, currentSites);
    if (this.currentSites_.length) {
      this.updateScrollableContents();
    }
  }

  private computeSubmitDisabled_() {
    this.submitDisabled = !this.selectedSites_.size;
  }

  // Convert iron-list index (0-indexed) to aria-posinset (1-indexed).
  private getAriaPosinset_(index: number): number {
    return index + 1;
  }

  // Called to recalculate checked status of entries when the site changes due
  // to list updates.
  private isSelectedSite_(site: Site) {
    return this.selectedSites_.has(site);
  }

  private onToggleSelection_(e: {model: {item: Site}, detail: boolean}) {
    if (e.detail) {
      this.selectedSites_.add(e.model.item);
    } else {
      this.selectedSites_.delete(e.model.item);
    }
    this.computeSubmitDisabled_();
  }

  submit() {
    assert(!this.submitDisabled);
    this.selectedSites_.forEach(rule => {
      this.setPrefDictEntry(
          TAB_DISCARD_EXCEPTIONS_PREF, rule, convertDateToWindowsEpoch());
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
