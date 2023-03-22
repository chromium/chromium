// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './log_object.html.js';
import {LogMessage, Severity} from './types.js';

Polymer({
  is: 'log-object',

  _template: getTemplate(),

  properties: {
    /**
     * Log whose metadata is displayed within this element.
     * @type {!LogMessage}
     */
    logMessage: {
      type: Object,
      observer: 'logMessageChanged_',
    },
  },

  /**
   * Sets the log message style based on severity level.
   * @private
   */
  logMessageChanged_() {
    switch (this.logMessage.severity) {
      case Severity.WARNING:
        this.className = 'warning-log';
        break;
      case Severity.ERROR:
        this.className = 'error-log';
        break;
      case Severity.VERBOSE:
        this.className = 'verbose-log';
        break;
      default:
        this.className = 'default-log';
        break;
    }
  },

  /**
   * @return {string}
   * @private
   */
  getFilenameWithLine_() {
    if (!this.logMessage) {
      return '';
    }

    // The filename is prefixed with "../../", so replace it with "//".
    const filename = this.logMessage.file.replace('../../', '//');
    return filename + ':' + this.logMessage.line;
  },
});
