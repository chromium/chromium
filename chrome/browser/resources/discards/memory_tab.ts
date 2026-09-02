// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getOrCreateDetailsProvider} from './discards.js';
import type {DetailsProviderRemote, ProcessDescription} from './discards.mojom-webui.js';
import type {ProcessMemoryDump} from './memory_instrumentation.mojom-webui.js';
import {SECTION_DEFS} from './memory_section_defs.js';
import {getCss} from './memory_tab.css.js';
import {getHtml} from './memory_tab.html.js';
import {SortedTableMixinLit} from './sorted_table_mixin_lit.js';

/**
 * Represents a single row in the expanded metric breakdown table for a process
 * (e.g. "Allocated Objects", "Main Heap: Old Space").
 */
export interface ProcessMetricRow {
  label: string;
  valueFormatted: string;
  magnitudePercent: number|null;
  history: Array<number|null>;
  maxScale?: number;
}

/**
 * Represents a collapsible group/card of related metrics (e.g. "OS Metrics",
 * "Malloc", "V8") within a process's expanded breakdown view.
 */
export interface MetricSection {
  id: string;
  label: string;
  totalFormatted: string|null;
  metrics: ProcessMetricRow[];
}

/**
 * Represents a top-level process row in the main memory table, containing
 * process metadata, top-level memory values, sparkline history, and child
 * metric breakdown sections.
 */
export interface ProcessDisplayRow {
  pid: number;
  description: string;
  privateFootprintKb: number;
  privateFootprintFormatted: string;
  magnitudePercent: number;
  history: Array<number|null>;
  sections: MetricSection[];
  isDead: boolean;
  hasDumpError?: boolean;
}

/**
 * Represents the display state and dimensions of a single bar within a
 * sparkline (trend line) visualization.
 */
export interface SparklineBar {
  height: number;
  value: number|null;
  isEmpty: boolean;
  isError: boolean;
}

/**
 * Retained state for a terminated process to allow its row and final frozen
 * history to linger in the UI for a brief period before removal.
 */
interface DeadProcessInfo {
  row: ProcessDisplayRow;
  deadTicksRemaining: number;
}

const HISTORY_LENGTH = 10;
const DEAD_TICKS_RETENTION = 5;
const REFRESH_INTERVAL_MS = 2000;

/**
 * Computes a percentage magnitude [1, 100] relative to a maximum value, or
 * returns 0 if the maximum is non-positive.
 */
function computeMagnitudePercent(valueKb: number, maxKb: number): number {
  return maxKb > 0 ?
      Math.min(100, Math.max(1, Math.round((valueKb / maxKb) * 100))) :
      0;
}

export function formatBytesInMb(bytes: number): string {
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

export function formatKibInMb(kib: number): string {
  return `${(kib / 1024).toFixed(1)} MB`;
}

const MemoryTabElementBase = SortedTableMixinLit(CrLitElement);

export class MemoryTabElement extends MemoryTabElementBase {
  static get is() {
    return 'memory-tab';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isPaused_: {type: Boolean},
      processes_: {type: Array},
      lastUpdated_: {type: String},
      maxPrivateFootprintKb_: {type: Number},
      expandedPids_: {type: Object},
      expandedSections_: {type: Object},
    };
  }

  // Whether polling for new memory dumps is paused.
  protected accessor isPaused_: boolean = false;
  // The list of process rows displayed in the table.
  protected accessor processes_: ProcessDisplayRow[] = [];
  // Formatted timestamp of the last successful data refresh.
  protected accessor lastUpdated_: string = '';
  // Maximum private memory footprint (in KiB) across active processes,
  // used as the global reference scale for sparklines and magnitude bars.
  protected accessor maxPrivateFootprintKb_: number = 1;
  // Set of process IDs whose metric breakdown sections are expanded.
  protected accessor expandedPids_: Set<number> = new Set();
  // Set of expanded section keys in the format `${pid}:${sectionId}`.
  protected accessor expandedSections_: Set<string> = new Set();

  // Remote interface for querying memory dump data from the browser process.
  private discardsDetailsProvider_: DetailsProviderRemote;
  // Timer ID for the recurring polling interval.
  private timerId_: number|null = null;
  // Guard to prevent overlapping fetch requests.
  private isFetching_: boolean = false;
  // Set of active PIDs from the previous update cycle, used to detect
  // process terminations.
  private previousActivePids_: Set<number> = new Set();
  // Map of PID -> (metric key -> history array) for tracking sub-metric
  // trend lines.
  private historyMap_: Map<number, Map<string, Array<number|null>>> = new Map();
  // Map of PID -> private footprint history array for top-level sparklines.
  private processHistoryMap_: Map<number, Array<number|null>> = new Map();
  // Map of PID -> DeadProcessInfo for retaining terminated processes.
  private deadProcessesMap_: Map<number, DeadProcessInfo> = new Map();

  constructor() {
    super();
    this.discardsDetailsProvider_ = getOrCreateDetailsProvider();
    this.sortKey = 'privateFootprintKb';
    this.sortReverse = true;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.startPolling_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopPolling_();
  }

  private startPolling_() {
    this.stopPolling_();
    this.fetchData_();
    this.timerId_ = window.setInterval(() => {
      if (!this.isPaused_) {
        this.fetchData_();
      }
    }, REFRESH_INTERVAL_MS);
  }

  private stopPolling_() {
    if (this.timerId_ !== null) {
      window.clearInterval(this.timerId_);
      this.timerId_ = null;
    }
  }

  protected onTogglePauseClick_() {
    this.isPaused_ = !this.isPaused_;
    if (!this.isPaused_) {
      this.fetchData_();
    }
  }

  protected onProcessRowClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const pid = Number(target.dataset['pid']);
    const nextPids = new Set(this.expandedPids_);
    const nextSections = new Set(this.expandedSections_);

    if (nextPids.has(pid)) {
      nextPids.delete(pid);
    } else {
      nextPids.add(pid);
      for (const section of SECTION_DEFS) {
        nextSections.add(`${pid}:${section.id}`);
      }
    }
    this.expandedPids_ = nextPids;
    this.expandedSections_ = nextSections;
  }

  protected onSectionRowClick_(e: Event) {
    e.stopPropagation();
    const target = e.currentTarget as HTMLElement;
    const pid = Number(target.dataset['pid']);
    const sectionId = target.dataset['sectionId']!;
    const key = `${pid}:${sectionId}`;
    const next = new Set(this.expandedSections_);
    if (next.has(key)) {
      next.delete(key);
    } else {
      next.add(key);
    }
    this.expandedSections_ = next;
  }

  protected isExpanded_(pid: number): boolean {
    return this.expandedPids_.has(pid);
  }

  protected isSectionExpanded_(pid: number, sectionId: string): boolean {
    return this.expandedSections_.has(`${pid}:${sectionId}`);
  }

  /**
   * Converts a history array of metric samples into 10 sparkline bar models.
   * Missing leading slots are filled with empty placeholder bars.
   *
   * @param history Array of historical samples (null = missing, <0 = error).
   * @param maxScale Optional reference maximum for scaling bar heights
   *     globally. If omitted, scales locally against the max valid sample.
   * @return Array of 10 SparklineBar objects ready for rendering.
   */
  getSparklineBars(history: Array<number|null>, maxScale?: number):
      SparklineBar[] {
    const validValues =
        history.filter((val): val is number => val !== null && val >= 0);
    const maxVal =
        maxScale ?? (validValues.length > 0 ? Math.max(...validValues, 1) : 1);
    const totalSlots = 10;
    const maxHeight = 18;

    const padded: Array<number|null> = [];
    const missing = Math.max(0, totalSlots - history.length);
    for (let i = 0; i < missing; i++) {
      padded.push(null);
    }
    for (const val of history) {
      padded.push(val);
    }

    return padded.map((val) => {
      if (val === null) {
        return {
          height: 2,
          value: null,
          isEmpty: true,
          isError: false,
        };
      }
      if (val < 0) {
        return {
          height: 8,
          value: null,
          isEmpty: false,
          isError: true,
        };
      }
      const barHeight = val > 0 && maxVal > 0 ?
          Math.min(
              maxHeight, Math.max(2, Math.round((val / maxVal) * maxHeight))) :
          2;
      return {
        height: barHeight,
        value: val,
        isEmpty: false,
        isError: false,
      };
    });
  }

  private async fetchData_() {
    if (this.isFetching_) {
      return;
    }
    this.isFetching_ = true;
    try {
      const response =
          await this.discardsDetailsProvider_.getProcessMemoryDumps();
      if (response.processDumps && response.processDumps.length > 0) {
        this.processData_(
            response.processDumps, response.processDescriptions || []);
        const now = new Date();
        this.lastUpdated_ = now.toLocaleTimeString();
      }
    } catch (e) {
      console.error('Failed to fetch process memory dumps', e);
    } finally {
      this.isFetching_ = false;
    }
  }

  /**
   * Safely extracts a numeric metric value from a process memory dump allocator
   * entry.
   *
   * @param dump The process memory dump containing allocator dumps.
   * @param dumpName Name of the allocator dump in chromeAllocatorDumps.
   * @param metricName Name of the numeric metric within numericEntries.
   * @return The numeric value if present, or null otherwise.
   */
  private getNumericEntryValue_(
      dump: ProcessMemoryDump, dumpName: string, metricName: string): number
      |null {
    if (!dump.chromeAllocatorDumps) {
      return null;
    }

    const allocatorDump = dump.chromeAllocatorDumps[dumpName];
    if (!allocatorDump || !allocatorDump.numericEntries) {
      return null;
    }

    const val = allocatorDump.numericEntries[metricName];
    return val !== undefined ? Number(val) : null;
  }

  private processData_(
      dumps: ProcessMemoryDump[], descriptions: ProcessDescription[]) {
    const descMap = new Map<number, string>();
    if (descriptions) {
      for (const desc of descriptions) {
        const pid = typeof desc.pid === 'object' && desc.pid !== null ?
            desc.pid.pid :
            Number(desc.pid);
        descMap.set(pid, desc.description);
      }
    }

    const dumpMap = new Map<number, ProcessMemoryDump>();
    for (const dump of dumps) {
      const pid = typeof dump.pid === 'object' && dump.pid !== null ?
          dump.pid.pid :
          Number(dump.pid);
      dumpMap.set(pid, dump);
    }

    // Process descriptions are the primary source of truth for active
    // processes. Also include any extra processes present in the dumps.
    const allPids = new Set<number>(descMap.keys());
    for (const pid of dumpMap.keys()) {
      allPids.add(pid);
    }

    const activePids = new Set<number>();
    const newProcessRows: ProcessDisplayRow[] = [];

    // Calculate max Private Footprint across all active processes for global
    // magnitude scaling.
    let maxPmfKb = 1;
    for (const dump of dumps) {
      if (dump.osDump) {
        const pmfKb = dump.osDump.privateFootprintKb;
        if (pmfKb > maxPmfKb) {
          maxPmfKb = pmfKb;
        }
      }
    }

    for (const pid of allPids) {
      activePids.add(pid);
      this.deadProcessesMap_.delete(pid);

      const dump = dumpMap.get(pid);
      const hasDump = Boolean(dump && dump.osDump);

      const privateFootprintKb = hasDump ? dump!.osDump.privateFootprintKb : -1;
      const historySample = hasDump ? privateFootprintKb : -1;

      // Update process history.
      let procHistory = this.processHistoryMap_.get(pid) || [];
      procHistory =
          [...procHistory.slice(-(HISTORY_LENGTH - 1)), historySample];
      this.processHistoryMap_.set(pid, procHistory);

      // Get or create metric history map for this PID.
      let pidMetricHistories = this.historyMap_.get(pid);
      if (!pidMetricHistories) {
        pidMetricHistories = new Map();
        this.historyMap_.set(pid, pidMetricHistories);
      }

      // Collect sections for this process.
      const sections: MetricSection[] = [];
      for (const sectionDef of SECTION_DEFS) {
        const metricRows: ProcessMetricRow[] = [];
        for (const def of sectionDef.metrics) {
          let rawVal: number|null = null;
          let formatted = '--';
          let magnitudePercent: number|null = null;

          if (hasDump) {
            if (def.dumpName === '') {
              if (def.metricName === 'private_footprint_kb') {
                rawVal = privateFootprintKb;
                formatted = formatKibInMb(rawVal);
                magnitudePercent = computeMagnitudePercent(rawVal, maxPmfKb);
              } else if (def.metricName === 'resident_set_kb') {
                rawVal = dump!.osDump.residentSetKb;
                formatted = formatKibInMb(rawVal);
                magnitudePercent = computeMagnitudePercent(rawVal, maxPmfKb);
              } else if (def.metricName === 'shared_footprint_kb') {
                rawVal = dump!.osDump.sharedFootprintKb;
                formatted = formatKibInMb(rawVal);
                magnitudePercent = computeMagnitudePercent(rawVal, maxPmfKb);
              }
            } else {
              rawVal = this.getNumericEntryValue_(
                  dump!, def.dumpName, def.metricName);
              if (rawVal !== null) {
                if (def.unit === 'bytes') {
                  formatted = formatBytesInMb(rawVal);
                  magnitudePercent =
                      computeMagnitudePercent(rawVal / 1024, maxPmfKb);
                } else if (def.unit === 'kib') {
                  formatted = formatKibInMb(rawVal);
                  magnitudePercent = computeMagnitudePercent(rawVal, maxPmfKb);
                } else if (def.unit === 'count') {
                  formatted = rawVal.toLocaleString();
                  magnitudePercent = null;
                } else if (def.unit === 'percent') {
                  formatted = `${rawVal.toFixed(1)}%`;
                  magnitudePercent = null;
                }
              }
            }
          }

          const metricKey = `${def.dumpName}:${def.metricName}`;
          let history = pidMetricHistories.get(metricKey) || [];
          const metricSample =
              (hasDump && rawVal !== null) ? rawVal : (hasDump ? null : -1);
          history = [...history.slice(-(HISTORY_LENGTH - 1)), metricSample];
          pidMetricHistories.set(metricKey, history);

          let maxScale: number|undefined = undefined;
          if (def.unit === 'kib') {
            maxScale = maxPmfKb;
          } else if (def.unit === 'bytes') {
            maxScale = maxPmfKb * 1024;
          } else if (def.unit === 'percent') {
            maxScale = 100;
          }

          // Only include metrics that have received at least one non-zero data
          // point in their history for this process.
          const hasReceivedData = history.some(val => val !== null && val > 0);
          if (!hasReceivedData) {
            continue;
          }

          metricRows.push({
            label: def.label,
            valueFormatted: formatted,
            magnitudePercent,
            history,
            maxScale,
          });
        }

        // Only include sections that have at least one metric with valid data.
        if (metricRows.length > 0) {
          let totalFormatted: string|null = null;
          if (['malloc', 'partition_alloc', 'blink_gc', 'v8'].includes(
                  sectionDef.id)) {
            const firstMetric = metricRows[0];
            if (firstMetric) {
              totalFormatted = firstMetric.valueFormatted;
            }
          }

          sections.push({
            id: sectionDef.id,
            label: sectionDef.label,
            totalFormatted,
            metrics: metricRows,
          });
        }
      }

      let desc = descMap.get(pid);
      if (!desc) {
        if (dump && dump.serviceName) {
          desc = `Service: ${dump.serviceName}`;
        } else {
          desc = `Process ${pid}`;
        }
      }

      const processMagnitude =
          hasDump ? computeMagnitudePercent(privateFootprintKb, maxPmfKb) : 0;

      const activeRow: ProcessDisplayRow = {
        pid,
        description: desc,
        privateFootprintKb,
        privateFootprintFormatted: hasDump ? formatKibInMb(privateFootprintKb) :
                                             '--',
        magnitudePercent: processMagnitude,
        history: procHistory,
        sections,
        isDead: false,
        hasDumpError: !hasDump,
      };

      newProcessRows.push(activeRow);
    }

    this.maxPrivateFootprintKb_ = maxPmfKb;

    // Register newly terminated processes that were active in the previous
    // cycle but are no longer active.
    for (const prevRow of this.processes_) {
      if (this.previousActivePids_.has(prevRow.pid) &&
          !activePids.has(prevRow.pid) &&
          !this.deadProcessesMap_.has(prevRow.pid)) {
        const deadRow: ProcessDisplayRow = {
          ...prevRow,
          isDead: true,
        };
        this.deadProcessesMap_.set(prevRow.pid, {
          row: deadRow,
          deadTicksRemaining: DEAD_TICKS_RETENTION,
        });
      }
    }
    this.previousActivePids_ = activePids;

    // Retain recently terminated processes for a fixed duration (frozen
    // history).
    for (const [pid, deadInfo] of this.deadProcessesMap_.entries()) {
      if (!activePids.has(pid)) {
        deadInfo.deadTicksRemaining--;
        if (deadInfo.deadTicksRemaining > 0) {
          newProcessRows.push(deadInfo.row);
        } else {
          this.deadProcessesMap_.delete(pid);
          this.processHistoryMap_.delete(pid);
          this.historyMap_.delete(pid);
        }
      }
    }

    this.processes_ = newProcessRows;
  }

  protected getSortedProcesses_(): ProcessDisplayRow[] {
    const list = [...this.processes_];
    list.sort((a, b) => {
      // Put dead processes after active processes unless sorting specifically
      // by pid/desc
      if (a.isDead !== b.isDead) {
        return a.isDead ? 1 : -1;
      }
      let cmp = 0;
      if (this.sortKey === 'pid') {
        cmp = a.pid - b.pid;
      } else if (this.sortKey === 'description') {
        cmp = a.description.localeCompare(b.description);
      } else {
        cmp = a.privateFootprintKb - b.privateFootprintKb;
      }
      return this.sortReverse ? -cmp : cmp;
    });
    return list;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-tab': MemoryTabElement;
  }
}

customElements.define(MemoryTabElement.is, MemoryTabElement);
