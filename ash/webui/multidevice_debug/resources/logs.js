/* Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @typedef {{
 *   text: string,
 *   time: string,
 *   file: string,
 *   line: number,
 *   severity: number,
 *}}
 */
let Log;

const Logs = {
  controller_: null,

  /**
   * Initializes the logs UI.
   */
  init: function() {
    Logs.controller_ = new LogsListController();

    const clearLogsButton = document.querySelector('#clear-logs-button');
    clearLogsButton.onclick = function() {
      WebUI.clearLogs();
    };

    const saveLogsButton = document.querySelector('#save-logs-button');
    saveLogsButton.onclick = () => {
      Logs.saveLogs();
    };

    WebUI.getLogMessages();
  },

  saveLogs: function() {
    const blob = new Blob(
        [document.querySelector('#logs-list').innerText],
        {type: 'text/plain;charset=utf-8'});
    const url = URL.createObjectURL(blob);

    const anchorEl = document.createElement('a');
    anchorEl.href = url;
    anchorEl.download = 'proximity_auth_logs_' + new Date().toJSON() + '.txt';
    document.body.appendChild(anchorEl);
    anchorEl.click();

    window.setTimeout(function() {
      document.body.removeChild(anchorEl);
      window.URL.revokeObjectURL(url);
    }, 0);
  },
};

/**
 * Interface with the native WebUI component for LogBuffer events. The functions
 * contained in this object will be invoked by the browser for each operation
 * performed on the native LogBuffer.
 */
const LogBufferInterface = {
  /**
   * Called when a new log message is added.
   * @param {!Log} log
   */
  onLogMessageAdded: function(log) {
    if (Logs.controller_) {
      Logs.controller_.add(log);
    }
  },

  /**
   * Called when the log buffer is cleared.
   */
  onLogBufferCleared: function() {
    if (Logs.controller_) {
      Logs.controller_.clear();
    }
  },

  /**
   * Called in response to chrome.send('getLogMessages') with the log messages
   * currently in the buffer.
   * @param {!Array<Log>} messages
   */
  onGotLogMessages: function(messages) {
    if (Logs.controller_) {
      Logs.controller_.set(messages);
    }
  },
};

/**
 * Controller for the logs list element, updating it based on user input and
 * logs received from native code.
 */
class LogsListController {
  constructor() {
    this.logsList_ = document.querySelector('#logs-list');
    this.itemTemplate_ = document.querySelector('#item-template');
    this.shouldSnapToBottom_ = true;

    this.logsList_.onscroll = this.onScroll_.bind(this);
  }

  /**
   * Listener for scroll event of the logs list element, used for snap to bottom
   * logic.
   */
  onScroll_() {
    const list = this.logsList_;
    this.shouldSnapToBottom_ =
        list.scrollTop + list.offsetHeight == list.scrollHeight;
  }

  /**
   * Clears all log items from the logs list.
   */
  clear() {
    const items = this.logsList_.querySelectorAll('.log-item');
    for (let i = 0; i < items.length; ++i) {
      items[i].remove();
    }
    this.shouldSnapToBottom_ = true;
  }

  /**
   * Adds a log to the logs list.
   * @param {!Log} log
   */
  add(log) {
    const directories = log.file.split('/');
    const source = directories[directories.length - 1] + ':' + log.line;

    const t = this.itemTemplate_.content;
    t.querySelector('.log-item').attributes.severity.value = log.severity;
    t.querySelector('.item-time').textContent = log.time;
    t.querySelector('.item-source').textContent = source;
    t.querySelector('.item-text').textContent = log.text;

    const newLogItem = document.importNode(this.itemTemplate_.content, true);
    this.logsList_.appendChild(newLogItem);
    if (this.shouldSnapToBottom_) {
      this.logsList_.scrollTop = this.logsList_.scrollHeight;
    }
  }

  /**
   * Initializes the log list from an array of logs.
   * @param {!Array<!Log>} logs
   */
  set(logs) {
    this.clear();
    for (let i = 0; i < logs.length; ++i) {
      this.add(logs[i]);
    }
  }
}
