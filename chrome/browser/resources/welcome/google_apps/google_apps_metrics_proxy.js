// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {ModuleMetricsProxyImpl, NuxGoogleAppsInteractions} from '../shared/module_metrics_proxy.js';

export class GoogleAppsMetricsProxyImpl extends ModuleMetricsProxyImpl {
  constructor() {
    super(
        'FirstRun.NewUserExperience.GoogleAppsInteraction',
        NuxGoogleAppsInteractions);
  }
}

addSingletonGetter(GoogleAppsMetricsProxyImpl);
