// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

import {SegmentationInternalsBrowserProxy} from './segmentation_internals_browser_proxy.js';

function getProxy(): SegmentationInternalsBrowserProxy {
  return SegmentationInternalsBrowserProxy.getInstance();
}

function initialize() {
  $('get-segment').onclick = async function() {
    const {result} = await getProxy().getSegment(
        ($('segment-key') as HTMLInputElement).value);
    $('is-ready').textContent = String(result.isReady);
    $('optimization-target').textContent = String(result.optimizationTarget);
  };

  getProxy().getCallbackRouter().onServiceStatusChanged.addListener(
      (initialized: boolean, status: number) => {
        $('initialized').textContent = String(initialized);
        $('service-status').textContent = String(status);
      });

  getProxy().getCallbackRouter().onSegmentInfoAvailable.addListener(
      () => {
          // TODO(qinmin): display the segment info on internal page.
      });

  getProxy().getServiceStatus();
}

document.addEventListener('DOMContentLoaded', initialize);
