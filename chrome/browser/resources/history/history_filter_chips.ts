/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// <if expr="_google_chrome">
import './internal/icons.html.js';
// </if>

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Device, TimePeriodOption} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './history_filter_chips.css.js';
import {getHtml} from './history_filter_chips.html.js';

export class HistoryFilterChipsElement extends CrLitElement {
  static get is() {
    return 'history-filter-chips';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showDeviceFilter: {type: Boolean},
      devices: {type: Array},
      timePeriodOptions: {type: Array},
      showTimePeriodFilter: {type: Boolean},
      showActorFilter: {type: Boolean},
      userVisits: {type: Boolean},
      actorVisits: {type: Boolean},
      clientIds: {type: Array},
    };
  }

  accessor showDeviceFilter: boolean = false;
  accessor devices: Device[] = [];
  accessor timePeriodOptions: TimePeriodOption[] = [];
  accessor showTimePeriodFilter: boolean = false;
  accessor showActorFilter: boolean = false;
  // These values are used to filter the history query; true indicates that
  // results for the respective value should be included.
  accessor userVisits: boolean = true;
  accessor actorVisits: boolean = true;
  accessor clientIds: string[] = [];

  protected isUserSelected(): boolean {
    return this.userVisits && !this.actorVisits;
  }

  protected isActorSelected(): boolean {
    return this.actorVisits && !this.userVisits;
  }

  protected isDeviceSelected_(device: Device): boolean {
    return this.clientIds.length > 0 &&
        this.clientIds.length === device.ids.length &&
        this.clientIds.every((id, index) => id === device.ids[index]);
  }

  protected hasActiveFilter_(): boolean {
    const hasActiveActorDropdownFilter =
        this.showActorFilterDropDownVersion_() &&
        (this.isUserSelected() || this.isActorSelected());
    return hasActiveActorDropdownFilter || this.clientIds.length > 0;
  }

  protected onActorSelectChange_(e: Event) {
    const value = (e.target as HTMLSelectElement).value;
    if (value === 'user') {
      this.userVisits = true;
      this.actorVisits = false;
    } else if (value === 'actor') {
      this.userVisits = false;
      this.actorVisits = true;
    } else {
      this.userVisits = true;
      this.actorVisits = true;
    }

    this.recordMetrics_(this.userVisits, this.actorVisits);
    this.fireChange_(this.userVisits, this.actorVisits);
  }

  protected onDeviceSelectChange_(e: Event) {
    const select = e.target as HTMLSelectElement;
    const index = Number(select.value);
    const device = index >= 0 ? this.devices[index] : undefined;
    this.clientIds = device ? device.ids : [];
    this.fireChange_(this.userVisits, this.actorVisits);
  }

  protected async onClearAllClick_() {
    this.userVisits = true;
    this.actorVisits = true;
    this.clientIds = [];

    this.fireChange_(this.userVisits, this.actorVisits);

    await this.updateComplete;
    this.focusFirstFilter_();
  }

  protected onUserVisitsClick_() {
    this.userVisits = true;
    this.actorVisits = !this.actorVisits;

    this.recordMetrics_(this.userVisits, this.actorVisits);
    this.fireChange_(this.userVisits, this.actorVisits);
  }

  protected onActorVisitsClick_() {
    this.actorVisits = true;
    this.userVisits = !this.userVisits;

    this.recordMetrics_(this.userVisits, this.actorVisits);
    this.fireChange_(this.userVisits, this.actorVisits);
  }

  protected recordMetrics_(newUserState: boolean, newActorState: boolean) {
    let action = '';
    if (newUserState && newActorState) {
      action = 'HistoryPage_ShowAllEnabled';
    } else if (newUserState && !newActorState) {
      action = 'HistoryPage_ShowUserOnlyEnabled';
    } else if (!newUserState && newActorState) {
      action = 'HistoryPage_ShowActorOnlyEnabled';
    }

    if (action !== '') {
      BrowserProxyImpl.getInstance().recordAction(action);
    }
  }

  protected getUserVisitsIcon_(): string {
    if (this.isUserSelected()) {
      return 'cr:check';
    }
    return this.isCriticalActionsEnabled_() ? 'cr:person' : 'cr:person-filled';
  }

  protected getActorVisitsIcon_(): string {
    if (this.isActorSelected()) {
      return 'cr:check';
    }
    // <if expr="_google_chrome">
    return this.isCriticalActionsEnabled_() ?
        'history-internal:arrow-selector-spark-new' :
        'history-internal:screensaver-auto';
    // </if>
    // <if expr="not _google_chrome">
    return '';
    // </if>
  }

  protected showActorFilterDropDownVersion_(): boolean {
    return loadTimeData.getBoolean('isBrowsingHistoryFilterByActorEnabled');
  }

  private isCriticalActionsEnabled_(): boolean {
    return loadTimeData.valueExists('isCriticalActionsEnabled') &&
        loadTimeData.getBoolean('isCriticalActionsEnabled');
  }

  private focusFirstFilter_() {
    this.shadowRoot.querySelector('select')?.focus();
  }

  private fireChange_(userVisits: boolean, actorVisits: boolean) {
    this.fire('filter-changed', {
      userVisits,
      actorVisits,
      clientIds: this.clientIds,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-filter-chips': HistoryFilterChipsElement;
  }
}

customElements.define(HistoryFilterChipsElement.is, HistoryFilterChipsElement);
