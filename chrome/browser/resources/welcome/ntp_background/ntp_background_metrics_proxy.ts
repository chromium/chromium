// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ModuleMetricsProxy} from '../shared/module_metrics_proxy.js';
import {ModuleMetricsProxyImpl, NuxNtpBackgroundInteractions} from '../shared/module_metrics_proxy.js';

export class NtpBackgroundMetricsProxyImpl extends ModuleMetricsProxyImpl {
  constructor() {
    super(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        NuxNtpBackgroundInteractions);
  }

  static getInstance(): ModuleMetricsProxy {
    return instance || (instance = new NtpBackgroundMetricsProxyImpl());
  }

  static setInstance(obj: ModuleMetricsProxy) {
    instance = obj;
  }
}

let instance: ModuleMetricsProxy|null = null;
