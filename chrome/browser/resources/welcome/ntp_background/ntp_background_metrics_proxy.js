// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  /** @return {!NtpBackgroundMetricsProxyImpl} */
  static getInstance() {
    return instance || (instance = new NtpBackgroundMetricsProxyImpl());
  }

  /** @param {!NtpBackgroundMetricsProxyImpl} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?NtpBackgroundMetricsProxyImpl} */
let instance = null;
