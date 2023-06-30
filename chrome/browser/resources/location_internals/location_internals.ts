// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnose_info_table.js';

import {$, getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {GeolocationInternalsRemote} from './geolocation_internals.mojom-webui.js';
import {LocationInternalsHandler} from './location_internals.mojom-webui.js';

export const WATCH_BUTTON_ID = 'watch-btn';
export const REFRESH_BUTTON_ID = 'refresh-btn';
export const REFRESH_STATUS_ID = 'refresh-status';
export const REFRESH_STATUS_SUCCESS =
    'GeolocationInternals Status:  Success, last update at ';
export const REFRESH_STATUS_FAILURE =
    `GeolocationInternals Status:  Error, Geolocation Service is not
     initialized, please click "Start Watching Position" button or access
      Geolocation API at least once.`;
export const REFRESH_FINISH_EVENT = 'refresh-finish-event';
const WATCH_TABLE_ID = 'watch-position-table';
const PROVIDER_STATE_TABLE_ID = 'provider-state-table';
const PROVIDER_STATE_ENUM = {
  0: 'Stop',
  1: 'High Accuracy',
  2: 'Low Accuracy',
  3: 'Blocked By System Permission',
};

let watchId: number = -1;
let geolocationInternals: GeolocationInternalsRemote|null = null;

function logSuccess(position: GeolocationPosition) {
  const data: Record<string, string> = {};
  data['timestamp'] = new Date(position.timestamp).toLocaleString();

  for (const key in position.coords) {
    const value = position.coords[key as keyof GeolocationCoordinates];
    if (typeof value === 'number' || typeof value === 'string') {
      data[key] = value.toString();
    }
  }
  updateTable(WATCH_TABLE_ID, [data]);
}

function logError(error: GeolocationPositionError) {
  const data: Record<string, string> = {};
  data['timestamp'] = new Date().toLocaleString();
  data['fail reason'] = `${error.message}, code: ${error.code}`;
  updateTable(WATCH_TABLE_ID, [data]);
}

function updateTable(name: string, data: Array<Record<string, string>>) {
  removeTable(name);
  const newTableElement = document.createElement('diagnose-info-table');
  newTableElement.id = name;
  newTableElement.createTableData(data);
  newTableElement.updateCaption(name);
  getRequiredElement<HTMLElement>('container').appendChild(newTableElement);
}

function removeTable(name: string) {
  const oldTableElement = $(name);
  if (oldTableElement) {
    oldTableElement.remove();
  }
}

// Initialize buttons callback
function initializeButtons() {
  const watchButton = getRequiredElement<HTMLElement>(WATCH_BUTTON_ID);
  watchButton.addEventListener('click', watchPosition);
  const refreshButton = getRequiredElement<HTMLElement>(REFRESH_BUTTON_ID);
  refreshButton.addEventListener('click', getDiagnostics);
}

// Initialize MOJO pipe
export function initializeMojo() {
  geolocationInternals = new GeolocationInternalsRemote();
  LocationInternalsHandler.getRemote().bindInternalsInterface(
      geolocationInternals.$.bindNewPipeAndPassReceiver());
}

function watchPosition() {
  const watchButton = getRequiredElement<HTMLElement>(WATCH_BUTTON_ID);
  if (watchId === -1) {
    watchId = navigator.geolocation.watchPosition(logSuccess, logError, {
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

async function getDiagnostics() {
  if (!geolocationInternals) {
    initializeMojo();
  }
  const refreshStatus = getRequiredElement(REFRESH_STATUS_ID);
  const {diagnostics} = await geolocationInternals!.getDiagnostics();
  if (diagnostics) {
    refreshStatus.textContent =
        REFRESH_STATUS_SUCCESS + new Date().toLocaleString();
    if ('providerState' in diagnostics) {
      updateTable(
          PROVIDER_STATE_TABLE_ID,
          [{'Provider State': PROVIDER_STATE_ENUM[diagnostics.providerState]}]);
    }
  } else {
    refreshStatus.textContent = REFRESH_STATUS_FAILURE;
  }

  const refreshButton = getRequiredElement(REFRESH_BUTTON_ID);
  refreshButton.dispatchEvent(new CustomEvent(REFRESH_FINISH_EVENT));
}

document.addEventListener('DOMContentLoaded', initializeButtons);
