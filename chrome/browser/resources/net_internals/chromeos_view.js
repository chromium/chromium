// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {DivView} from './view.js';

/**
 * This view displays information on ChromeOS specific features.
 */
export class CrosView extends DivView {
  constructor() {
    // Call superclass's constructor.
    super(CrosView.MAIN_BOX_ID);
  }
}

CrosView.TAB_ID = 'tab-handle-chromeos';
CrosView.TAB_NAME = 'ChromeOS';
CrosView.TAB_HASH = '#chromeos';

CrosView.MAIN_BOX_ID = 'chromeos-view-tab-content';

addSingletonGetter(CrosView);
