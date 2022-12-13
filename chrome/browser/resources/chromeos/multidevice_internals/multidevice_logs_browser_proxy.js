// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';
import {LogMessage} from './types.js';

/**
 * JavaScript hooks into the native WebUI handler to pass LogMessages to the
 * logging tab.
 */
export class MultideviceLogsBrowserProxy {
  /**
   * @return {!Promise<!Array<!LogMessage>>}
   */
  getLogMessages() {
    return sendWithPromise('getMultideviceLogMessages');
  }
}

addSingletonGetter(MultideviceLogsBrowserProxy);
