// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ClientInfo, SegmentInfo} from './segmentation_internals.mojom-webui.js';
import {SegmentationInternalsBrowserProxy} from './segmentation_internals_browser_proxy.js';

function getProxy(): SegmentationInternalsBrowserProxy {
  return SegmentationInternalsBrowserProxy.getInstance();
}

// Checks the given URL for expected pattern, and allowed list of param.
function getSafeURL(untrustedURL: URL|undefined): URL|undefined {
  if (untrustedURL === undefined) {
    return undefined;
  }
  const allowedList = [
    'https://www.google.com/search',
    'https://surveys.qualtrics.com/jfe/form/SV_cNMWDhaegCrsrsy',
  ];
  const allowedParams = ['ID', 'SG', 'Q_CHL', 'Q_DL', '_g_', 'QRID', 'CR', 'q'];
  const withoutParams: string = untrustedURL.origin + untrustedURL.pathname;
  if (!allowedList.includes(withoutParams)) {
    return undefined;
  }

  const trustedURL = new URL(withoutParams);
  untrustedURL.searchParams.forEach((value, key) => {
    if (allowedParams.includes(key)) {
      trustedURL.searchParams.append(key, value);
    }
  });
  return trustedURL;
}

function openSurvey(result: string|undefined) {
  try {
    const untrustedParam =
        new URL(window.location.href).searchParams.get('url');
    if (untrustedParam) {
      const untrustedURL = new URL(decodeURIComponent(untrustedParam));
      const trustedURL = getSafeURL(untrustedURL);
      if (trustedURL) {
        if (result) {
          trustedURL.searchParams.append('CR', encodeURIComponent(result));
        }
        window.location.href = trustedURL.toString();
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
