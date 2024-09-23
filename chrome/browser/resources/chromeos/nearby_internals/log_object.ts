// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './log_object.html.js';
import type {LogMessage} from './types.js';
import {Severity} from './types.js';


/** @polymer */
class LogObjectElement extends PolymerElement {
  static get is() {
    return 'log-object';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Log whose metadata is displayed within this element.
       */
      logMessage: {
        type: Object,
        observer: 'logMessageChanged_',
      },
    };
  }

  logMessage: LogMessage;

  /**
   * Sets the log message style based on severity level.
   */
  private logMessageChanged_(): void {
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
  }

  private getFilenameWithLine(): string {
    if (!this.logMessage) {
      return '';
    }

    // The filename is prefixed with "../../", so replace it with "//".
    const filename = this.logMessage.file.replace('../../', '//');
    return filename + ':' + this.logMessage.line;
  }
}

customElements.define(LogObjectElement.is, LogObjectElement);
