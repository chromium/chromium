// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../../dom.js';
import {ViewName} from '../../type.js';
import {View} from '../view.js';

/**
 * Base controller of settings view.
 */
export class BaseSettings extends View {
  constructor(name: ViewName) {
    super(name, {
      dismissByEsc: true,
      dismissByBackgroundClick: true,
      dismissOnStopStreaming: true,
    });

    dom.getFrom(this.root, '.menu-header button', HTMLButtonElement)
        .addEventListener('click', () => this.leave());
  }
}
