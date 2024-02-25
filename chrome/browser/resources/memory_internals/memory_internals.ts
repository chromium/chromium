// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

type Process = [number, string, boolean];

interface ProcessList {
  message: string;
  processes: Process[];
}

function requestProcessList() {
  sendWithPromise('requestProcessList').then(onProcessListReceived);
}

function saveDump() {
  chrome.send('saveDump');
}

function startProfiling(pid: number) {
  // After profiling starts, the browser will send an updated process list.
  sendWithPromise('startProfiling', pid).then(onProcessListReceived);
}

// celltype should either be "td" or "th". The contents of the |cols| will be
// added as children of each table cell if they are non-null.
function addListRow(
    table: HTMLElement, celltype: string, cols: Array<Text|HTMLElement|null>) {
  const tr = document.createElement('tr');
  for (const col of cols) {
    const cell = document.createElement(celltype);
    if (col) {
      cell.appendChild(col);
    }
    tr.appendChild(cell);
  }
  table.appendChild(tr);
}

function onProcessListReceived(data: ProcessList) {
  getRequiredElement('message').innerText = data['message'];

  const proclist = getRequiredElement('proclist');
  proclist.innerText = '';  // Clear existing contents.

  const processes = data['processes'];
  if (processes.length === 0) {
    return;
  }  // No processes to dump, don't make the table.

  const table = document.createElement('table');

  // Heading.
  addListRow(table, 'th', [
    null,
    document.createTextNode('Process ID'),
    document.createTextNode('Name'),
  ]);

  for (const proc of processes) {
    const procId = proc[0];

    const procIdText = document.createTextNode(procId.toString());
    const description = document.createTextNode(proc[1]);
    const profiled = proc[2];

    const button = document.createElement('button');
    if (profiled) {
      button.innerText = 'Profiling...';
    } else {
      button.innerText = '\u2600 Start profiling';
      button.onclick = () => startProfiling(procId);
    }

    addListRow(table, 'td', [button, procIdText, description]);
  }

  proclist.appendChild(table);
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', () => {
  getRequiredElement('refresh').onclick = requestProcessList;
  getRequiredElement('save').onclick = saveDump;

  addWebUiListener('save-dump-progress', (progress: string) => {
    getRequiredElement('save-dump-text').innerText = progress;
  });

  requestProcessList();
});

/* For manual testing.
function fakeResults() {
  onProcessListReceived([
    [ 11234, "Process 11234 [Browser]" ],
    [ 11235, "Process 11235 [Renderer]" ],
    [ 11236, "Process 11236 [Renderer]" ]]);
}
document.addEventListener('DOMContentLoaded', fakeResults);
*/
