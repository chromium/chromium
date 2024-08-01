// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {ClientInfo, SegmentInfo} from './segmentation_internals.mojom-webui.js';
import {SegmentationInternalsBrowserProxy} from './segmentation_internals_browser_proxy.js';

function getProxy(): SegmentationInternalsBrowserProxy {
  return SegmentationInternalsBrowserProxy.getInstance();
}

function isURLSafe(urlStr: string|undefined): boolean {
  if (urlStr === undefined) {
    return false;
  }
  const allowedList = ['https://www.google.com/search?q=chrome'];
  for (let i = 0; i < allowedList.length; ++i) {
    if (urlStr === allowedList[i]) {
      return true;
    }
  }
  return false;
}

function openSurvey(result: string|undefined) {
  try {
    const untrustedParam =
        new URL(window.location.href).searchParams.get('url');
    if (untrustedParam) {
      const untruedtURL = new URL(decodeURIComponent(untrustedParam));
      const untrustedURLStr = untruedtURL ? untruedtURL.toString() : undefined;
      if (isURLSafe(untrustedURLStr)) {
        let safeURL: string = untrustedURLStr!;
        if (result) {
          safeURL = safeURL! + '&option=' + encodeURIComponent(result);
        }
        window.location.href = safeURL;
      }
    }
  } catch (error) {
  }
  const div = getRequiredElement('client-container');
  div.innerHTML =
      getTrustedHTML`Failed to open. Please check instructions in email.`;
}

function processPredictionResult(segmentInfo: SegmentInfo) {
  const result = String(segmentInfo.predictionResult) +
      ' Timestamp: ' + String(segmentInfo.predictionTimestamp.internalValue);
  const encoded = window.btoa(result);
  openSurvey(encoded);
}

function processClientInfo(info: ClientInfo) {
  for (let i = 0; i < info.segmentInfo.length; ++i) {
    processPredictionResult(info.segmentInfo[i]!);
  }
}

function initialize() {
  const div = getRequiredElement('client-container');
  div.innerHTML = getTrustedHTML`Loading...`;
  setTimeout(() => {
    openSurvey(undefined);
  }, 1000);

  getProxy().getCallbackRouter().onClientInfoAvailable.addListener(
      (clientInfos: ClientInfo[]) => {
        for (let i = 0; i < clientInfos.length; ++i) {
          if (clientInfos[i]!.segmentationKey === 'metrics_clustering') {
            processClientInfo(clientInfos[i]!);
          }
        }
      });

  getProxy().getServiceStatus();
}

document.addEventListener('DOMContentLoaded', initialize);
