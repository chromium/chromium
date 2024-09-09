// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
        return true;
      }
    }
  } catch (error) {
  }
  return false;
}

function openError() {
  window.location.href = 'chrome://network-error/-404';
}

function processPredictionResult(segmentInfo: SegmentInfo) {
  const result = String(segmentInfo.predictionResult) +
      ' Timestamp: ' + String(segmentInfo.predictionTimestamp.internalValue);
  const encoded = window.btoa(result);
  if (!openSurvey(encoded)) {
    openError();
  }
}

function processClientInfo(info: ClientInfo) {
  for (let i = 0; i < info.segmentInfo.length; ++i) {
    processPredictionResult(info.segmentInfo[i]!);
  }
}

function initialize() {
  // Timeout to wait for segmentation results.
  const timeoutSec = 3000;
  setTimeout(() => {
    openError();
  }, timeoutSec);

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
