// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {ModuleMetricsProxyImpl, NuxNtpBackgroundInteractions} from '../shared/module_metrics_proxy.js';

export class NtpBackgroundMetricsProxyImpl extends ModuleMetricsProxyImpl {
  constructor() {
    super(
        'FirstRun.NewUserExperience.NtpBackgroundInteraction',
        NuxNtpBackgroundInteractions);
  }

  getInteractions() {
    return this.interactions_;
  }
}

addSingletonGetter(NtpBackgroundMetricsProxyImpl);
