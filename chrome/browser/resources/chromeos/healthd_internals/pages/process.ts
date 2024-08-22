// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_input/cr_input.js';

import {sendWithPromise} from '//resources/js/cr.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HealthdApiProcessInfo, HealthdApiProcessResult} from '../externs.js';

import {getTemplate} from './process.html.js';
import {HealthdInternalsPage} from './utils/page_interface.js';
import {UiUpdateHelper} from './utils/ui_update_helper.js';

/**
 * The data structure for each row in process table.
 */
interface DisplayedProcessInfo {
  // Process ID (PID).
  processId: number;
  // Filename of the executable, which will be `<unknown>` if the data from
  // healthd is null.
  name: string;
  // Process priority.
  priority: number;
  // Nice value of the process.
  nice: number;
  // Process state.
  state: string;
  // Number of threads in the process.
  threadsNumber: number;
  // User the process is running as.
  userId: number;
  // PID of the parent of this process.
  parentProcessId: number;
  // Group ID of this process.
  processGroupId: number;
  // Amount of resident memory currently used by the process, in KiB.
  residentMemoryKib: number;
  // Uptime of the process, in clock ticks.
  uptimeTicks: number;
  // Attempted count of read syscalls.
  readSystemCallsCount: number;
  // Attempted count of write syscalls.
  writeSystemCallsCount: number;
  // Command which started the process.
  command: string;
}

function filterProcessData(data: HealthdApiProcessInfo[], filterQuery: string):
    HealthdApiProcessInfo[] {
  if (filterQuery === '') {
    return data;
  }

  return data.filter((process: HealthdApiProcessInfo) => {
    const displayedName: string =
        (process.name === undefined) ? '<unknown>' : process.name;
    if (displayedName.includes(filterQuery)) {
      return true;
    }

    for (const stringField
             of [process.processId, process.state, process.threadsNumber,
                 process.parentProcessId, process.processGroupId,
                 process.readSystemCallsCount, process.writeSystemCallsCount,
                 process.command]) {
      if (stringField.includes(filterQuery)) {
        return true;
      }
    }

    for (const numberField of [process.nice, process.priority]) {
      if (numberField.toString().includes(filterQuery)) {
        return true;
      }
    }

    return false;
  });
}

function convertProcessData(processes: HealthdApiProcessInfo[]):
    DisplayedProcessInfo[] {
  return processes.map(
      (item: HealthdApiProcessInfo) => ({
        processId: parseInt(item.processId),
        name: (item.name === undefined) ? '<unknown>' : item.name,
        priority: item.priority,
        nice: item.nice,
        state: item.state,
        threadsNumber: parseInt(item.threadsNumber),
        userId: parseInt(item.userId),
        parentProcessId: parseInt(item.parentProcessId),
        processGroupId: parseInt(item.processGroupId),
        residentMemoryKib: parseInt(item.residentMemoryKib),
        uptimeTicks: parseInt(item.uptimeTicks),
        readSystemCallsCount: parseInt(item.readSystemCallsCount),
        writeSystemCallsCount: parseInt(item.writeSystemCallsCount),
        command: item.command,
      }));
}

export class HealthdInternalsProcessElement extends PolymerElement implements
    HealthdInternalsPage {
  static get is() {
    return 'healthd-internals-process';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      processData: {type: Array},
      filterQuery: {type: String},
      displayedData: {
        type: Array,
        computed: 'getDisplayedData(processData, filterQuery)',
      },
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    // Process data is not used in line charts, so we only need to use the same
    // UI update frequency to fetch process data here and update process page.
    this.updateHelper = new UiUpdateHelper(() => {
      sendWithPromise('getHealthdProcessInfo')
          .then((data: HealthdApiProcessResult) => {
            this.processData = data.processes;
          });
    });
  }

  // Latest process data from healthd.
  private processData: HealthdApiProcessInfo[] = [];

  // The user entered filter query.
  private filterQuery: string = '';

  // Data displayed in the process table.
  private displayedData: DisplayedProcessInfo[] = [];

  // Helper for updating UI regularly. Init in `connectedCallback`.
  private updateHelper: UiUpdateHelper;

  updateVisibility(isVisible: boolean) {
    this.updateHelper.updateVisibility(isVisible);
  }

  updateUiUpdateInterval(intervalSeconds: number) {
    this.updateHelper.updateUiUpdateInterval(intervalSeconds);
  }

  private getDisplayedData(data: HealthdApiProcessInfo[], filterQuery: string):
      DisplayedProcessInfo[] {
    return convertProcessData(filterProcessData(data, filterQuery));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-process': HealthdInternalsProcessElement;
  }
}

customElements.define(
    HealthdInternalsProcessElement.is, HealthdInternalsProcessElement);
