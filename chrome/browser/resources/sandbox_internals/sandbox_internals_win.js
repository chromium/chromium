// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   processId: number,
 *   processType: string,
 *   name: string,
 *   metricsName: string
 * }}
 */
let BrowserHostProcess;

/**
 * @typedef {{
 *   processId: number
 * }}
 */
let RendererHostProcess;

/**
 * This may have additional fields displayed in the JSON output.
 * See //sandbox/win/src/sandbox_constants.cc for keys in policy.
 * @typedef {{
 *   processIds: !Array<number>,
 *   lockdownLevel: string,
 *   desiredIntegrityLevel: string,
 *   platformMitigations: string
 * }}
 */
let PolicyDiagnostic;

/**
 * @typedef {{
 *   browser: !Array<!BrowserHostProcess>,
 *   renderer: !Array<!RendererHostProcess>,
 *   policies: !Array<!PolicyDiagnostic>
 * }}
 */
let SandboxDiagnostics;

/**
 * Adds a row to the sandbox-status table.
 * @param {!Array<string>} args
 */
function addRow(args) {
  const row = document.createElement('tr');
  for (const text of args) {
    const col = row.appendChild(document.createElement('td'));
    col.textContent = text;
  }
  $('sandbox-status').appendChild(row);
}

/**
 * Adds policy information for a process to the sandbox-status table.
 * @param {number} pid
 * @param {string} type
 * @param {string} name
 * @param {PolicyDiagnostic} policy
 */
function addRowForProcess(pid, type, name, policy) {
  if (policy) {
    addRow([
      pid, type, name, policy.lockdownLevel, policy.desiredIntegrityLevel,
      policy.platformMitigations
    ]);
  } else {
    addRow([pid, type, name, 'Not Sandboxed', '', '']);
  }
}

/** @param {!SandboxDiagnostics} results */
function onGetSandboxDiagnostics(results) {
  // Make it easy to look up policies.
  /** @type {!Map<number,!PolicyDiagnostic>} */
  const policies = new Map();
  for (const policy of results.policies) {
    // At present only one process per TargetPolicy object.
    const pid = policy.processIds[0];
    policies.set(pid, policy);
  }

  // Titles.
  addRow(['Process', 'Type', 'Name', 'Sandbox', 'Intregity', 'Mitigations']);

  // Browser Processes.
  for (const process of results.browser) {
    const pid = process.processId;
    const name = process.name || process.metricsName;
    addRowForProcess(pid, process.processType, name, policies.get(pid));
  }

  // Renderer Processes.
  for (const process of results.renderer) {
    const pid = process.processId;
    addRowForProcess(pid, 'Renderer', '', policies.get(pid));
  }

  // Raw Diagnostics.
  $('raw-info').textContent =
      'policies: ' + JSON.stringify(results.policies, null, 2);
}

document.addEventListener('DOMContentLoaded', () => {
  cr.sendWithPromise('requestSandboxDiagnostics').then(onGetSandboxDiagnostics);
});
