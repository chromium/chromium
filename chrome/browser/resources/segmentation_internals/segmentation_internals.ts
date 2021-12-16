// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

import {SegmentInfo} from './segmentation_internals.mojom-webui.js';
import {SegmentationInternalsBrowserProxy} from './segmentation_internals_browser_proxy.js';

function getProxy(): SegmentationInternalsBrowserProxy {
  return SegmentationInternalsBrowserProxy.getInstance();
}

function addChildDivToParent(parent: HTMLElement, info: SegmentInfo) {
  const div = document.createElement('div');
  div.className = 'segment';
  div.textContent = String(info.optimizationTarget);
  div.setAttribute('simple', '');
  div.addEventListener('click', () => {
    if (div.hasAttribute('simple')) {
      div.textContent = String(info.segmentData);
      div.removeAttribute('simple');
    } else {
      div.textContent = String(info.optimizationTarget);
      div.setAttribute('simple', '');
    }
  });
  parent.appendChild(div);
}

function initialize() {
  getProxy().getCallbackRouter().onServiceStatusChanged.addListener(
      (initialized: boolean, status: number) => {
        $('initialized').textContent = String(initialized);
        $('service-status').textContent = String(status);
      });

  getProxy().getCallbackRouter().onSegmentInfoAvailable.addListener(
      (segmentInfos: Array<SegmentInfo>) => {
        const parent = $('segment-container');
        // Remove all current children.
        while (parent.firstChild) {
          parent.removeChild(parent.firstChild);
        }
        // Append new children.
        for (let i = 0; i < segmentInfos.length; ++i) {
          addChildDivToParent(parent, segmentInfos[i]!);
        }
      });

  getProxy().getServiceStatus();
}

document.addEventListener('DOMContentLoaded', initialize);
