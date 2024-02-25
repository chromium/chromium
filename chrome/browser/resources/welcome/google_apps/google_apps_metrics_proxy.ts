// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ModuleMetricsProxy} from '../shared/module_metrics_proxy.js';
import {ModuleMetricsProxyImpl, NuxGoogleAppsInteractions} from '../shared/module_metrics_proxy.js';

export class GoogleAppsMetricsProxyImpl extends ModuleMetricsProxyImpl {
  constructor() {
    super(
        'FirstRun.NewUserExperience.GoogleAppsInteraction',
        NuxGoogleAppsInteractions);
  }

  static getInstance(): ModuleMetricsProxy {
    return instance || (instance = new GoogleAppsMetricsProxyImpl());
  }

  static setInstance(obj: ModuleMetricsProxy) {
    instance = obj;
  }
}

let instance: ModuleMetricsProxy|null = null;
