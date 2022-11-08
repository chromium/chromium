// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';

import {createElementFromText} from './utils.js';

/**
 * Handles per-task log event.
 */
function onTaskLogRecorded(taskLog: {
  duration: number,
  task_description: string,
  result_description: string,
  details: string[],
}) {
  const details = document.createElement('td');
  details.classList.add('task-log-details');

  const label = document.createElement('label');
  details.appendChild(label);

  const collapseCheck = document.createElement('input');
  collapseCheck.setAttribute('type', 'checkbox');
  collapseCheck.classList.add('task-log-collapse-check');
  label.appendChild(collapseCheck);

  const ul = document.createElement('ul');
  for (let i = 0; i < taskLog.details.length; ++i) {
    ul.appendChild(createElementFromText('li', taskLog.details[i]!));
  }
  label.appendChild(ul);

  const tr = document.createElement('tr');
  tr.appendChild(createElementFromText(
      'td', taskLog.duration.toString(), {'class': 'task-log-duration'}));
  tr.appendChild(createElementFromText(
      'td', taskLog.task_description, {'class': 'task-log-description'}));
  tr.appendChild(createElementFromText(
      'td', taskLog.result_description, {'class': 'task-log-result'}));
  tr.appendChild(details);

  const entries = document.querySelector<HTMLElement>('#task-log-entries');
  assert(entries);
  entries.appendChild(tr);
}

/**
 * Get initial sync service values and set listeners to get updated values.
 */
function main() {
  addWebUiListener('task-log-recorded', onTaskLogRecorded);
  chrome.send('observeTaskLog');
}

document.addEventListener('DOMContentLoaded', main);
