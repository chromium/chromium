// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

let watchButton: HTMLElement;
let watchTable: HTMLElement;
let watchId: number = -1;

document.addEventListener('DOMContentLoaded', () => {
  watchButton = getRequiredElement<HTMLElement>('watch-btn');
  watchTable = getRequiredElement<HTMLElement>('watch-position');
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
  assert(watchTable);
  const timeCell = getRequiredElement<HTMLElement>('watch-position-timestamp');
  const positionCell =
      getRequiredElement<HTMLElement>('watch-position-position');

  timeCell.textContent = new Date(position.timestamp).toLocaleString();
  positionCell.textContent =
      `${position.coords.latitude} ° , ${position.coords.longitude} ° `;

  if (position.coords.accuracy) {
    const accuracyCell =
        getRequiredElement<HTMLElement>('watch-position-accuracy');
    accuracyCell.textContent = position.coords.accuracy.toString();
  }

  if (position.coords.altitude) {
    const altitudeCell =
        getRequiredElement<HTMLElement>('watch-position-altitude');
    altitudeCell.textContent = position.coords.altitude.toString();
  }

  if (position.coords.altitudeAccuracy) {
    const altitudeAccuracyCell =
        getRequiredElement<HTMLElement>('watch-position-altitude-accuracy');
    altitudeAccuracyCell.textContent =
        position.coords.altitudeAccuracy.toString();
  }

  if (position.coords.heading) {
    const headingCell =
        getRequiredElement<HTMLElement>('watch-position-heading');
    headingCell.textContent = position.coords.heading.toString();
  }

  if (position.coords.speed) {
    const speedCell = getRequiredElement<HTMLElement>('watch-position-speed');
    speedCell.textContent = position.coords.speed.toString();
  }
}

function logError(error: GeolocationPositionError) {
  assert(watchTable);
  const timeCell = getRequiredElement<HTMLElement>('watch-position-timestamp');
  const positionCell =
      getRequiredElement<HTMLElement>('watch-position-position');
  timeCell.textContent = new Date().toLocaleString();
  positionCell.textContent = `${error.message}, code: ${error.code}`;
}
