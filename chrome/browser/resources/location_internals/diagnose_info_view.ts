// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnose_info_table.js';

import {CustomElement} from '//resources/js/custom_element.js';

import {DiagnoseInfoTableElement} from './diagnose_info_table.js';
import {getTemplate} from './diagnose_info_view.html.js';
import {GeolocationDiagnostics} from './geolocation_internals.mojom-webui.js';

const PROVIDER_STATE_TABLE_ID = 'provider-state-table';
const PROVIDER_STATE_ENUM = {
  0: 'Stop',
  1: 'High Accuracy',
  2: 'Low Accuracy',
  3: 'Blocked By System Permission',
};
const WATCH_TABLE_ID = 'watch-position-table';

export class DiagnoseInfoViewElement extends CustomElement {
  static get is() {
    return 'diagnose-info-view';
  }

  static override get template() {
    return getTemplate();
  }

  watchPositionSuccess = (position: GeolocationPosition) => {
    const data: Record<string, string> = {};
    data['timestamp'] = new Date(position.timestamp).toLocaleString();

    for (const key in position.coords) {
      const value = position.coords[key as keyof GeolocationCoordinates];
      if (typeof value === 'number' || typeof value === 'string') {
        data[key] = value.toString();
      }
    }
    this.updateWatchPositionTable(data);
  };

  watchPositionError = (error: GeolocationPositionError) => {
    const data: Record<string, string> = {};
    data['timestamp'] = new Date().toLocaleString();
    data['fail reason'] = `${error.message}, code: ${error.code}`;
    this.updateWatchPositionTable(data);
  };

  private providerStateTable_: DiagnoseInfoTableElement;
  private watchPositionTable_: DiagnoseInfoTableElement;

  constructor() {
    super();
    this.providerStateTable_ =
        this.getRequiredElement<DiagnoseInfoTableElement>(
            `#${PROVIDER_STATE_TABLE_ID}`);
    this.watchPositionTable_ =
        this.getRequiredElement<DiagnoseInfoTableElement>(`#${WATCH_TABLE_ID}`);
  }

  updateDiagnosticsTables(data: GeolocationDiagnostics) {
    if ('providerState' in data) {
      this.providerStateTable_.updateTable(
          PROVIDER_STATE_TABLE_ID,
          [{'Provider State': PROVIDER_STATE_ENUM[data.providerState]}]);
    }
  }

  updateWatchPositionTable(data: Record<string, string>) {
    this.watchPositionTable_.updateTable(WATCH_TABLE_ID, [data]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnose-info-view': DiagnoseInfoViewElement;
  }
}

customElements.define(DiagnoseInfoViewElement.is, DiagnoseInfoViewElement);
