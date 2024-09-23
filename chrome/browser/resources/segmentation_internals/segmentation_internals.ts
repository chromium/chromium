// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {ClientInfo, SegmentInfo} from './segmentation_internals.mojom-webui.js';
import {SegmentationInternalsBrowserProxy} from './segmentation_internals_browser_proxy.js';

function getProxy(): SegmentationInternalsBrowserProxy {
  return SegmentationInternalsBrowserProxy.getInstance();
}

function addClientInfo(parent: HTMLElement, info: ClientInfo) {
  const div = document.createElement('div');
  div.className = 'client';
  const configTitle = document.createElement('h5');
  configTitle.textContent =
      'Segmentation Key: ' + String(info.segmentationKey) +
      ', Selected Segment: ' + String(info.selectedSegment);
  div.appendChild(configTitle);
  for (let i = 0; i < info.segmentInfo.length; ++i) {
    addSegmentInfoToParent(div, info.segmentationKey, info.segmentInfo[i]!);
  }
  parent.appendChild(div);
}

function addSegmentInfoToParent(
    parent: HTMLElement, segmentationKey: string, info: SegmentInfo) {
  const div = document.createElement('div');
  div.className = 'segment';
  const targetDiv = document.createElement('div');
  targetDiv.textContent = 'Segment Id: ' + String(info.segmentName);
  div.appendChild(targetDiv);
  const resultDiv = document.createElement('div');
  resultDiv.textContent = 'Result: ' + String(info.predictionResult) +
      ' Time: ' + String(info.predictionTimestamp.internalValue);
  div.appendChild(resultDiv);
  const buttonDiv = document.createElement('div');
  if (info.canExecuteSegment) {
    const btn = document.createElement('button');
    btn.innerHTML = getTrustedHTML`Execute model`;
    btn.addEventListener('click', () => {
      getProxy().executeModel(info.segmentId);
    });
    buttonDiv.appendChild(btn);
  }
  const overwriteText = document.createElement('label');
  overwriteText.innerHTML = getTrustedHTML`Overwrite result: `;
  buttonDiv.appendChild(overwriteText);
  const overwriteValue = document.createElement('input');
  overwriteValue.type = 'number';
  overwriteValue.value = '0';
  overwriteValue.className = 'overwrite';
  buttonDiv.appendChild(overwriteValue);
  const overwriteBtn = document.createElement('button');
  overwriteBtn.innerHTML = getTrustedHTML`Overwrite`;
  overwriteBtn.addEventListener('click', () => {
    getProxy().overwriteResult(
        info.segmentId, parseFloat(overwriteValue.value));
  });
  buttonDiv.appendChild(overwriteBtn);
  const setSelectionBtn = document.createElement('button');
  setSelectionBtn.innerHTML = getTrustedHTML`Set Selected`;
  setSelectionBtn.addEventListener('click', () => {
    getProxy().setSelected(segmentationKey, info.segmentId);
  });
  buttonDiv.appendChild(setSelectionBtn);
  div.appendChild(buttonDiv);
  const dataDiv = document.createElement('div');
  dataDiv.textContent = String(info.segmentData);
  div.appendChild(dataDiv);
  dataDiv.className = 'hidden-meta';
  div.setAttribute('simple', '');
  div.addEventListener('click', (e) => {
    if (e.target !== targetDiv && e.target !== resultDiv &&
        e.target !== dataDiv) {
      return;
    }
    if (div.hasAttribute('simple')) {
      dataDiv.className = 'shown-meta';
      div.removeAttribute('simple');
    } else {
      dataDiv.className = 'hidden-meta';
      div.setAttribute('simple', '');
    }
  });
  parent.appendChild(div);
}

function initialize() {
  getProxy().getCallbackRouter().onServiceStatusChanged.addListener(
      (initialized: boolean, status: number) => {
        getRequiredElement('initialized').textContent = String(initialized);
        getRequiredElement('service-status').textContent = String(status);
      });

  getProxy().getCallbackRouter().onClientInfoAvailable.addListener(
      (clientInfos: ClientInfo[]) => {
        const parent = getRequiredElement('client-container');
        // Remove all current children.
        while (parent.firstChild) {
          parent.removeChild(parent.firstChild);
        }
        // Append new children.
        for (let i = 0; i < clientInfos.length; ++i) {
          addClientInfo(parent, clientInfos[i]!);
        }
      });

  getProxy().getServiceStatus();
}

document.addEventListener('DOMContentLoaded', initialize);
