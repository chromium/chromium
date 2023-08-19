// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnose_info_table.js';

import {CustomElement} from '//resources/js/custom_element.js';
import {Time, TimeDelta} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {DiagnoseInfoTableElement} from './diagnose_info_table.js';
import {getTemplate} from './diagnose_info_view.html.js';
import {GeolocationDiagnostics, INVALID_CHANNEL, INVALID_RADIO_SIGNAL_STRENGTH, INVALID_SIGNAL_TO_NOISE, NetworkLocationDiagnostics, PositionCacheDiagnostics, WifiPollingPolicyDiagnostics} from './geolocation_internals.mojom-webui.js';
import {BAD_ACCURACY, BAD_ALTITUDE, BAD_HEADING, BAD_LATITUDE_LONGITUDE, BAD_SPEED, GeopositionResult} from './geoposition.mojom-webui.js';

export const PROVIDER_STATE_TABLE_ID = 'provider-state-table';
const PROVIDER_STATE_ENUM: {[key: number]: string} = {
  0: 'Stop',
  1: 'High Accuracy',
  2: 'Low Accuracy',
  3: 'Blocked By System Permission',
};
export const WATCH_TABLE_ID = 'watch-position-table';
export const WIFI_DATA_TABLE_ID = 'wifi-data-table';
export const POSITION_CACHE_TABLE_ID = 'position-cache-table';
export const WIFI_POLLING_POLICY_TABLE_ID = 'wifi-polling-policy-table';

// Converts `mojoTime` from `mojom_base.mojom.Time` to `Date`.
function mojoTimeToDate(mojoTime: Time) {
  // The Javascript `Date()` is based off of the number of milliseconds since
  // the UNIX epoch (1970-01-01 00::00:00 UTC), while `internalValue``
  // of the `base::Time` (represented in mojom.Time) represents the
  // number of microseconds since the Windows FILETIME epoch
  // (1601-01-01 00:00:00 UTC). This computes the final Javascript time by
  // computing the epoch delta and the conversion from microseconds to
  // milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // `epochDeltaInMs` is equal to `base::Time::kTimeTToMicrosecondsOffset`.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;
  return new Date(timeInMs - epochDeltaInMs);
}

// Returns a string representation of `mojoTime`.
function stringifyMojoTime(mojoTime?: Time) {
  if (!mojoTime) {
    return 'None';
  }
  return mojoTimeToDate(mojoTime).toLocaleString();
}

// Returns a string representation of `mojoGeopositionResult`.
function stringifyMojoGeopositionResult(
    mojoGeopositionResult?: GeopositionResult) {
  if (!mojoGeopositionResult) {
    return 'None';
  }
  if ('position' in mojoGeopositionResult) {
    const mojoGeoposition = mojoGeopositionResult.position;
    if (mojoGeoposition!.latitude === BAD_LATITUDE_LONGITUDE ||
        mojoGeoposition!.longitude === BAD_LATITUDE_LONGITUDE) {
      return 'Invalid geoposition';
    }
    const components = [];
    let latLong =
        `${mojoGeoposition!.latitude}°, ${mojoGeoposition!.longitude}°`;
    if (mojoGeoposition!.accuracy !== BAD_ACCURACY) {
      latLong += ` ±${mojoGeoposition!.accuracy} m`;
    }
    components.push(latLong);
    if (mojoGeoposition!.altitude !== BAD_ALTITUDE) {
      let altitude = `${mojoGeoposition!.altitude} m`;
      if (mojoGeoposition!.altitudeAccuracy !== BAD_ACCURACY) {
        altitude += ` ±${mojoGeoposition!.altitudeAccuracy} m`;
      }
      components.push(altitude);
    }
    if (mojoGeoposition!.heading !== BAD_HEADING) {
      components.push(`${mojoGeoposition!.heading}°`);
    }
    if (mojoGeoposition!.speed !== BAD_SPEED) {
      components.push(`${mojoGeoposition!.speed} m/s`);
    }
    components.push(stringifyMojoTime(mojoGeoposition!.timestamp));
    return components.join('; ');
  }
  if ('error' in mojoGeopositionResult) {
    const mojoGeopositionError = mojoGeopositionResult.error;
    return `${mojoGeopositionError!.errorMessage} (${
        mojoGeopositionError!.errorCode})`;
  }
  return 'Invalid result';
}

// Return a string representation of `TimeDelta` in second.
function stringifyMojoTimeDelta(mojoTime: TimeDelta|undefined) {
  if (!mojoTime) {
    return 'None';
  }
  return `${Number(mojoTime.microseconds) / 1000000}`;
}

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
  private wifiDataTable_: DiagnoseInfoTableElement;
  private positionCacheTable_: DiagnoseInfoTableElement;
  private watchPositionTable_: DiagnoseInfoTableElement;
  private wifiPollingPolicyTable_: DiagnoseInfoTableElement;

  constructor() {
    super();
    this.providerStateTable_ =
        this.getRequiredElement<DiagnoseInfoTableElement>(
            `#${PROVIDER_STATE_TABLE_ID}`);
    this.wifiDataTable_ = this.getRequiredElement<DiagnoseInfoTableElement>(
        `#${WIFI_DATA_TABLE_ID}`);
    this.positionCacheTable_ =
        this.getRequiredElement<DiagnoseInfoTableElement>(
            `#${POSITION_CACHE_TABLE_ID}`);
    this.watchPositionTable_ =
        this.getRequiredElement<DiagnoseInfoTableElement>(`#${WATCH_TABLE_ID}`);
    this.wifiPollingPolicyTable_ =
        this.getRequiredElement<DiagnoseInfoTableElement>(
            `#${WIFI_POLLING_POLICY_TABLE_ID}`);
  }

  updateDiagnosticsTables(data: GeolocationDiagnostics) {
    this.updateProviderState(data.providerState);
    this.updateNetworkLocationDiagnostics(data.networkLocationDiagnostics);
    this.updatePositionCacheDiagnostics(data.positionCacheDiagnostics);
    this.updateWifiPollingPolicyTable(data.wifiPollingPolicyDiagnostics);
  }

  updateProviderState(providerState: number) {
    let providerStateString = PROVIDER_STATE_ENUM[providerState];
    if (providerStateString === undefined) {
      providerStateString = 'Invalid state';
    }
    this.providerStateTable_.updateTable(
        PROVIDER_STATE_TABLE_ID, [{'Provider State': providerStateString}]);
  }

  updateNetworkLocationDiagnostics(networkLocationDiagnostics?:
                                       NetworkLocationDiagnostics) {
    if (!networkLocationDiagnostics) {
      this.wifiDataTable_.hideTable();
      return;
    }
    const wifiData: Array<Record<string, string>> = [];
    if (networkLocationDiagnostics.accessPointData !== null) {
      for (const accessPointData of
               networkLocationDiagnostics.accessPointData) {
        const row: Record<string, string> = {};
        row['MAC address'] = accessPointData.macAddress;
        if (accessPointData.radioSignalStrength ===
            INVALID_RADIO_SIGNAL_STRENGTH) {
          row['Signal strength'] = 'N/A';
        } else {
          row['Signal strength'] = `${accessPointData.radioSignalStrength} dBm`;
        }
        if (accessPointData.channel === INVALID_CHANNEL) {
          row['Channel'] = 'N/A';
        } else {
          row['Channel'] = accessPointData.channel.toString();
        }
        if (accessPointData.signalToNoise === INVALID_SIGNAL_TO_NOISE) {
          row['Signal to Noise Ratio'] = 'N/A';
        } else {
          row['Signal to Noise Ratio'] = `${accessPointData.signalToNoise} dB`;
        }
        if (accessPointData.timestamp) {
          row['Timestamp'] = stringifyMojoTime(accessPointData.timestamp);
        } else {
          row['Timestamp'] = 'N/A';
        }
        wifiData.push(row);
      }
      if (wifiData.length === 0) {
        // TODO: Hide the table when there is no access point data.
        const row: Record<string, string> = {};
        row['MAC address'] = 'No access point data';
        row['Signal strength'] = '';
        row['Channel'] = '';
        row['Signal to Noise Ratio'] = '';
        row['Timestamp'] = '';
        wifiData.push(row);
      }
    }
    let footerMessage;
    if (networkLocationDiagnostics.wifiTimestamp === null) {
      footerMessage = 'No Wi-Fi data received';
    } else {
      footerMessage = `Wi-Fi data last received ${
          stringifyMojoTime(networkLocationDiagnostics.wifiTimestamp)}`;
    }
    this.wifiDataTable_.updateTable(
        WIFI_DATA_TABLE_ID, wifiData, footerMessage);
  }

  updatePositionCacheDiagnostics(positionCacheDiagnostics?:
                                     PositionCacheDiagnostics) {
    if (!positionCacheDiagnostics) {
      this.positionCacheTable_.hideTable();
      return;
    }
    const row: Record<string, string> = {};
    row['Cache size'] = positionCacheDiagnostics.cacheSize.toString();
    row['Last cache hit'] = stringifyMojoTime(positionCacheDiagnostics.lastHit);
    row['Last cache miss'] =
        stringifyMojoTime(positionCacheDiagnostics.lastMiss);
    if (!positionCacheDiagnostics.hitRate) {
      row['Cache hit rate'] = 'N/A';
    } else {
      row['Cache hit rate'] = `${positionCacheDiagnostics.hitRate * 100}%`;
    }
    row['Last result'] = stringifyMojoGeopositionResult(
        positionCacheDiagnostics.lastNetworkResult);
    this.positionCacheTable_.updateTable(POSITION_CACHE_TABLE_ID, [row]);
  }

  updateWatchPositionTable(data: Record<string, string>) {
    const footerMessage = `Last updated ${new Date().toLocaleString()}`;
    this.watchPositionTable_.updateTable(WATCH_TABLE_ID, [data], footerMessage);
  }

  updateWifiPollingPolicyTable(data?: WifiPollingPolicyDiagnostics) {
    if (!data) {
      this.wifiPollingPolicyTable_.hideTable();
      return;
    }
    const row: Record<string, string> = {};
    row['Interval start time'] = stringifyMojoTime(data.intervalStart);
    row['Interval duration (sec)'] =
        stringifyMojoTimeDelta(data.intervalDuration);
    row['Polling interval (sec)'] =
        stringifyMojoTimeDelta(data.pollingInterval);
    row['Default interval (sec)'] =
        stringifyMojoTimeDelta(data.defaultInterval);
    row['No change interval (sec)'] =
        stringifyMojoTimeDelta(data.noChangeInterval);
    row['Two no change interval (sec)'] =
        stringifyMojoTimeDelta(data.twoNoChangeInterval);
    row['No Wi-Fi interval (sec)'] =
        stringifyMojoTimeDelta(data.noWifiInterval);
    this.wifiPollingPolicyTable_.updateTable(
        WIFI_POLLING_POLICY_TABLE_ID, [row]);
  }

  outputTables(): Record<string, any> {
    const tables = this.$all<DiagnoseInfoTableElement>('diagnose-info-table');
    const output: Record<string, any> = {};
    output['LocationInternals'] = [];
    for (const table of tables) {
      if (!table.visible()) {
        continue;
      }
      output['LocationInternals'].push(table.outputTable());
    }
    return output;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnose-info-view': DiagnoseInfoViewElement;
  }
}

customElements.define(DiagnoseInfoViewElement.is, DiagnoseInfoViewElement);
