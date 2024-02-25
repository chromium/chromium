// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnose_info_view.js';

import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {DiagnoseInfoViewElement} from './diagnose_info_view.js';
import type {AccessPointData, GeolocationDiagnostics, GeolocationInternalsObserverInterface, NetworkLocationResponse} from './geolocation_internals.mojom-webui.js';
import {GeolocationInternalsObserverReceiver, GeolocationInternalsRemote} from './geolocation_internals.mojom-webui.js';
import {LocationInternalsHandler} from './location_internals.mojom-webui.js';

export const WATCH_BUTTON_ID = 'watch-btn';
export const LOG_BUTTON_ID = 'log-btn';
export const REFRESH_STATUS_ID = 'refresh-status';
export const REFRESH_STATUS_SUCCESS = 'Last updated ';
export const REFRESH_STATUS_UNINITIALIZED =
    `Geolocation API is not initialized. Click "Start Watching Position" to
     begin.`;
export const REFRESH_FINISH_EVENT = 'refresh-finish-event';
export const DIAGNOSE_INFO_VIEW_ID = 'diagnose-info-view';

let watchId: number = -1;
let geolocationInternals: GeolocationInternalsRemote|undefined;
let geolocationInternalsObserver: GeolocationInternalsObserverReceiver|
    undefined;
const diagnoseInfoView =
    getRequiredElement<DiagnoseInfoViewElement>(DIAGNOSE_INFO_VIEW_ID);

class GeolocationInternalsObserver implements
    GeolocationInternalsObserverInterface {
  onDiagnosticsChanged(diagnostics: GeolocationDiagnostics) {
    handleDiagnosticsChanged(diagnostics);
  }
  onNetworkLocationRequested(request: AccessPointData[]) {
    handleNetworkLocationRequested(request);
  }
  onNetworkLocationReceived(response: NetworkLocationResponse|null) {
    handleNetworkLocationReceived(response);
  }
}

// Initialize buttons callback
function initializeButtons() {
  const watchButton = getRequiredElement<HTMLElement>(WATCH_BUTTON_ID);
  watchButton.addEventListener('click', watchPosition);
  const saveButton = getRequiredElement<HTMLElement>(LOG_BUTTON_ID);
  saveButton.addEventListener('click', saveDiagnostics);
}

// Initialize Mojo pipe
export function initializeMojo() {
  geolocationInternals = new GeolocationInternalsRemote();
  LocationInternalsHandler.getRemote().bindInternalsInterface(
      geolocationInternals.$.bindNewPipeAndPassReceiver());

  geolocationInternalsObserver = new GeolocationInternalsObserverReceiver(
      new GeolocationInternalsObserver());
  geolocationInternals!
      .addInternalsObserver(
          geolocationInternalsObserver.$.bindNewPipeAndPassRemote())
      .then(data => {
        handleDiagnosticsChanged(data.diagnostics);
      });
}

function watchPosition() {
  const watchButton = getRequiredElement<HTMLElement>(WATCH_BUTTON_ID);
  if (watchId === -1) {
    watchId = navigator.geolocation.watchPosition(
        diagnoseInfoView.watchPositionSuccess,
        diagnoseInfoView.watchPositionError, {
          enableHighAccuracy: true,
          timeout: 5000,
          maximumAge: 0,
        });
    watchButton.textContent = 'Stop Watching Position';
  } else {
    navigator.geolocation.clearWatch(watchId);
    watchId = -1;
    watchButton.textContent = 'Start Watching Position';
  }
}

function handleDiagnosticsChanged(diagnostics: GeolocationDiagnostics|null) {
  const refreshStatus = getRequiredElement(REFRESH_STATUS_ID);
  if (diagnostics) {
    refreshStatus.textContent =
        REFRESH_STATUS_SUCCESS + new Date().toLocaleString();
    diagnoseInfoView.updateDiagnosticsTables(diagnostics);
  } else {
    refreshStatus.textContent = REFRESH_STATUS_UNINITIALIZED;
  }
  window.dispatchEvent(new CustomEvent(REFRESH_FINISH_EVENT));
}

function handleNetworkLocationRequested(request: AccessPointData[]) {
  diagnoseInfoView.updateLastNetworkRequestTable(request);
  window.dispatchEvent(new CustomEvent(REFRESH_FINISH_EVENT));
}

function handleNetworkLocationReceived(response: NetworkLocationResponse|null) {
  diagnoseInfoView.updateLastNetworkResponseTable(response);
  window.dispatchEvent(new CustomEvent(REFRESH_FINISH_EVENT));
}

function saveDiagnostics() {
  const tables = diagnoseInfoView.outputTables();
  const content = JSON.stringify(tables, null, 2);
  const blob = new Blob([content], {type: 'application/json'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `location_internals_${new Date().toISOString()}.json`;
  a.click();
}

document.addEventListener('DOMContentLoaded', () => {
  initializeButtons();
  initializeMojo();
});
