// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../site_favicon.js';
import './tab_discard_exception_current_sites_entry.js';

import {PrefsMixin, PrefsMixinInterface} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrScrollableMixin, CrScrollableMixinInterface} from 'chrome://resources/cr_elements/cr_scrollable_mixin.js';
import {ListPropertyUpdateMixin, ListPropertyUpdateMixinInterface} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PerformanceBrowserProxy, PerformanceBrowserProxyImpl} from './performance_browser_proxy.js';
import {HighEfficiencyModeExceptionListAction, PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './tab_discard_exception_current_sites_list.html.js';
import {TAB_DISCARD_EXCEPTIONS_PREF} from './tab_discard_exception_validation_mixin.js';

export interface TabDiscardExceptionCurrentSitesListElement {
  $: {
    list: IronListElement,
  };
}

type Site = string;

type Constructor<T> = new (...args: any[]) => T;
const TabDiscardExceptionCurrentSitesListElementBase =
    ListPropertyUpdateMixin(CrScrollableMixin(PrefsMixin(PolymerElement))) as
    Constructor<ListPropertyUpdateMixinInterface&CrScrollableMixinInterface&
                PrefsMixinInterface&PolymerElement>;

export class TabDiscardExceptionCurrentSitesListElement extends
    TabDiscardExceptionCurrentSitesListElementBase {
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
        new Set(this.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value);
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
      this.appendPrefListItem(TAB_DISCARD_EXCEPTIONS_PREF, rule);
    });
    this.metricsProxy_.recordExceptionListAction(
        HighEfficiencyModeExceptionListAction.ADD_FROM_CURRENT);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-current-sites-list':
        TabDiscardExceptionCurrentSitesListElement;
  }
}

customElements.define(
    TabDiscardExceptionCurrentSitesListElement.is,
    TabDiscardExceptionCurrentSitesListElement);
