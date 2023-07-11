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

      selectedSites_: {type: Array, value: []},

      submitDisabled: {
        type: Boolean,
        computed: 'computeSubmitDisabled_(selectedSites_.length)',
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

  private currentSites_: string[];
  private selectedSites_: string[];
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
    if (this.updateIntervalID_ === undefined) {
      this.updateCurrentSites_().then(() => {
        this.updateIntervalID_ = setInterval(
            this.updateCurrentSites_.bind(this), this.updateIntervalMS_);
      });
    }
  }

  private stopUpdatingCurrentSites_() {
    if (this.updateIntervalID_ !== undefined) {
      clearInterval(this.updateIntervalID_);
      this.updateIntervalID_ = undefined;
    }
  }

  setUpdateIntervalForTesting(updateIntervalMS: number) {
    this.updateIntervalMS_ = updateIntervalMS;
  }

  getIsUpdatingForTesting() {
    return this.updateIntervalID_ !== undefined;
  }

  private async updateCurrentSites_() {
    const currentSites = await this.browserProxy_.getCurrentOpenSites();
    const existingSites =
        new Set(this.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value);
    this.updateList(
        'currentSites_', x => x,
        currentSites.filter(rule => !existingSites.has(rule)));
    if (this.currentSites_.length) {
      this.updateScrollableContents();
    }
  }

  private computeSubmitDisabled_() {
    return !this.selectedSites_.length;
  }

  private onToggleSelection_(e: {model: {index: number}}) {
    this.$.list.toggleSelectionForIndex(e.model.index);
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
