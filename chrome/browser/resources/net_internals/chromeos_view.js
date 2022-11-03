// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DivView} from './view.js';

/** @type {?CrosView} */
let instance = null;

/**
 * This view displays information on ChromeOS specific features.
 */
export class CrosView extends DivView {
  constructor() {
    // Call superclass's constructor.
    super(CrosView.MAIN_BOX_ID);
  }

  static getInstance() {
    return instance || (instance = new CrosView());
  }
}

CrosView.TAB_ID = 'tab-handle-chromeos';
CrosView.TAB_NAME = 'ChromeOS';
CrosView.TAB_HASH = '#chromeos';

CrosView.MAIN_BOX_ID = 'chromeos-view-tab-content';
