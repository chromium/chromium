// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordDuration, recordLoadDuration} from '../metrics_utils.js';
import {WindowProxy} from '../window_proxy.js';

/**
 * @fileoverview Provides the module descriptor. Each module must create a
 * module descriptor and register it at the NTP.
 */

export type InitializeModuleCallback = () =>
    Promise<HTMLElement[]|HTMLElement|null>;

export interface Module {
  elements: HTMLElement[];
  descriptor: ModuleDescriptor;
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

  /**
   * Initializes the module and returns one or more module elements on success.
   * @param timeout Timeout in milliseconds after which initialization aborts.
   * @param onNtpLoad `true` if the module is being initialized during the
   *     initial NTP load, `false` if it's being initialized later in the NTP's
   *     lifecycle.
   */
  async initialize(timeout: number, onNtpLoad: boolean = true):
      Promise<HTMLElement[]|HTMLElement|null> {
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

    const histogramBase = onNtpLoad ? 'NewTabPage.Modules.LoadedOnNTPLoad' :
                                      'NewTabPage.Modules.LoadedAfterNTPLoad';
    recordLoadDuration(`${histogramBase}`, loadEndTime);
    recordLoadDuration(`${histogramBase}.${this.id_}`, loadEndTime);
    return element;
  }
}
