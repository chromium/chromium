// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordDuration, recordLoadDuration} from '../metrics_utils.js';
import {WindowProxy} from '../window_proxy.js';

/**
 * @fileoverview Provides the module descriptor. Each module must create a
 * module descriptor and register it at the NTP.
 */

export type InitializeModuleCallback = () => Promise<HTMLElement|null>;

export type InitializeModuleCallbackV2 = () => Promise<HTMLElement>;

export interface Module {
  element: HTMLElement;
  descriptor: ModuleDescriptor;
}

export enum ModuleHeight {
  DYNAMIC = -1,
  SHORT = 166,
  TALL = 358,
}

export class ModuleDescriptor {
  private id_: string;
  private initializeCallback_: InitializeModuleCallback;

  constructor(id: string, initializeCallback: InitializeModuleCallback) {
    this.id_ = id;
    this.initializeCallback_ = initializeCallback;
  }

  get id(): string {
    return this.id_;
  }

  get height(): ModuleHeight {
    return ModuleHeight.DYNAMIC;
  }

  /**
   * Initializes the module and returns the module element on success.
   * @param timeout Timeout in milliseconds after which initialization aborts.
   */
  async initialize(timeout: number): Promise<HTMLElement|null> {
    const loadStartTime = WindowProxy.getInstance().now();
    const element = await Promise.race([
      this.initializeCallback_(),
      new Promise<null>(resolve => {
        WindowProxy.getInstance().setTimeout(() => {
          resolve(null);
        }, timeout);
      }),
    ]);
    if (!element) {
      return null;
    }
    const loadEndTime = WindowProxy.getInstance().now();
    const duration = loadEndTime - loadStartTime;
    recordLoadDuration('NewTabPage.Modules.Loaded', loadEndTime);
    recordLoadDuration(`NewTabPage.Modules.Loaded.${this.id_}`, loadEndTime);
    recordDuration('NewTabPage.Modules.LoadDuration', duration);
    recordDuration(`NewTabPage.Modules.LoadDuration.${this.id_}`, duration);
    return element;
  }
}

export class ModuleDescriptorV2 extends ModuleDescriptor {
  private height_: ModuleHeight;

  constructor(
      id: string, height: ModuleHeight,
      initializeCallback: InitializeModuleCallbackV2) {
    super(id, initializeCallback);
    this.height_ = height;
  }

  override get height() {
    return this.height_;
  }

  /**
   * Like |ModuleDescriptor.initialize()| but returns an empty element on
   * timeout.
   */
  override async initialize(timeout: number): Promise<HTMLElement> {
    return (await super.initialize(timeout)) || document.createElement('div');
  }
}
