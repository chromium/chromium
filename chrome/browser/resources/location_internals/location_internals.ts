// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnose_info_view.js';

import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {DiagnoseInfoViewElement} from './diagnose_info_view.js';
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
export const DIAGNOSE_INFO_VIEW_ID = 'diagnose-info-view';

let watchId: number = -1;
let geolocationInternals: GeolocationInternalsRemote|null = null;
const diagnoseInfoView =
    getRequiredElement<DiagnoseInfoViewElement>(DIAGNOSE_INFO_VIEW_ID);

// Initialize buttons callback
function initializeButtons() {
  const watchButton = getRequiredElement<HTMLElement>(WATCH_BUTTON_ID);
  watchButton.addEventListener('click', watchPosition);
  const refreshButton = getRequiredElement<HTMLElement>(REFRESH_BUTTON_ID);
  refreshButton.addEventListener('click', getDiagnostics);
}

// Initialize Mojo pipe
export function initializeMojo() {
  geolocationInternals = new GeolocationInternalsRemote();
  LocationInternalsHandler.getRemote().bindInternalsInterface(
      geolocationInternals.$.bindNewPipeAndPassReceiver());
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

async function getDiagnostics() {
  if (!geolocationInternals) {
    initializeMojo();
  }
  const refreshStatus = getRequiredElement(REFRESH_STATUS_ID);
  const {diagnostics} = await geolocationInternals!.getDiagnostics();
  if (diagnostics) {
    refreshStatus.textContent =
        REFRESH_STATUS_SUCCESS + new Date().toLocaleString();
    diagnoseInfoView.updateDiagnosticsTables(diagnostics);
  } else {
    refreshStatus.textContent = REFRESH_STATUS_FAILURE;
  }

  const refreshButton = getRequiredElement(REFRESH_BUTTON_ID);
  refreshButton.dispatchEvent(new CustomEvent(REFRESH_FINISH_EVENT));
}

document.addEventListener('DOMContentLoaded', initializeButtons);
