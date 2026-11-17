// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface OnDeviceAiEnabled {
  enabled: boolean;
  allowedByPolicy: boolean;
}

// TODO(crbug.com/573643364): use Linter after all model status are added.
// Mirrors optimization_guide::OnDeviceModelControl::ModelStatus in
// components/optimization_guide/core/model_execution/on_device_capability.h.
export enum OnDeviceModelStatus {
  READY = 0,
  DOWNLOADABLE = 1,
}

// An on-device AI model supported on this device.
export interface OnDeviceAiCapability {
  // OnDeviceBaseModel enum value identifying the model.
  id: number;
  status: OnDeviceModelStatus;
  categoryTitle: string;
  description: string;
  modelName: string;
  // Human readable, already formatted size (e.g. "2.0 GB"), or "0 B" when not
  // ready.
  size: string;
  // Bullet points describing what the model is used for, shown when the row
  // is expanded.
  details: string[];
}

export interface OnDeviceAiBrowserProxy {
  getOnDeviceAiEnabled(): Promise<OnDeviceAiEnabled>;
  setOnDeviceAiEnabled(enabled: boolean): void;
  openFeedbackDialog(): void;
  getOnDeviceAiCapabilities(): Promise<OnDeviceAiCapability[]>;
  // Resolves with the updated list of supported models after the deletion.
  deleteOnDeviceAiCapability(id: number): Promise<OnDeviceAiCapability[]>;
}

export class OnDeviceAiBrowserProxyImpl implements OnDeviceAiBrowserProxy {
  getOnDeviceAiEnabled(): Promise<OnDeviceAiEnabled> {
    return sendWithPromise('getOnDeviceAiEnabled');
  }

  setOnDeviceAiEnabled(enabled: boolean): void {
    chrome.send('setOnDeviceAiEnabled', [enabled]);
  }

  openFeedbackDialog(): void {
    chrome.send('openOnDeviceAiFeedbackDialog');
  }

  getOnDeviceAiCapabilities(): Promise<OnDeviceAiCapability[]> {
    return sendWithPromise('getOnDeviceAiCapabilities');
  }

  deleteOnDeviceAiCapability(id: number): Promise<OnDeviceAiCapability[]> {
    return sendWithPromise('deleteOnDeviceAiCapability', id);
  }

  static getInstance(): OnDeviceAiBrowserProxy {
    return instance || (instance = new OnDeviceAiBrowserProxyImpl());
  }

  static setInstance(obj: OnDeviceAiBrowserProxy) {
    instance = obj;
  }
}

let instance: OnDeviceAiBrowserProxy|null = null;
