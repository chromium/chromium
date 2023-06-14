// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnose_info_table.js';

import {$, getRequiredElement} from 'chrome://resources/js/util_ts.js';

const WATCH_TABLE_ID = 'watch-position-table';

let watchId: number = -1;

document.addEventListener('DOMContentLoaded', () => {
  const watchButton = getRequiredElement<HTMLElement>('watch-btn');
  watchButton.addEventListener('click', () => {
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
  });
});

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
