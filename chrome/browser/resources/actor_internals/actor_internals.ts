// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Typescript for actor_internals.html, served from chrome://actor-internals/
 * This is used to debug actor events recording. It displays a live
 * stream of all actor events that occur in chromium while the
 * chrome://actor-internals/ page is open.
 */

import {getRequiredElement} from '//resources/js/util.js';

import type {JournalEntry} from './actor_internals.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';

/**
 * Manages the state and rendering of the actor event log table.
 * Encapsulates all state (active spans, collapse state, indentation)
 * instead of using global variables.
 */
class ActorEventLog {
  private table: HTMLTableElement;
  private template: HTMLTemplateElement;
  private spanIdCounter: number = 0;
  private activeSpans: Array<{id: number, track: string, event: string}> = [];
  private spanState =
      new Map<number, {collapsed: boolean, parentIds: number[]}>();
  private indentationByTrack = new Map<string, number>();

  constructor() {
    this.table = getRequiredElement<HTMLTableElement>('actor-events-table');
    this.template = getRequiredElement<HTMLTemplateElement>('actor-events-row');
  }

  // --- State Helper Methods ---

  private getState(row: HTMLTableRowElement) {
    const rowSpanId = Number(row.dataset['spanId']);
    return this.spanState.get(rowSpanId);
  }

  private getParentIds(row: HTMLTableRowElement): number[] {
    const state = this.getState(row);
    return state ? state.parentIds : [];
  }

  isCollapsed(row: HTMLTableRowElement): boolean {
    const state = this.getState(row);
    return state ? state.collapsed : false;
  }

  /** Checks if any parent of a given row is currently collapsed. */
  private isAnyParentCollapsed(row: HTMLTableRowElement): boolean {
    for (const parentId of this.getParentIds(row)) {
      const parentState = this.spanState.get(parentId);
      if (parentState && parentState.collapsed) {
        return true;
      }
    }
    return false;
  }

  private getIndentation(track: string): number {
    return this.indentationByTrack.get(track) || 0;
  }

  private incrementIndentation(track: string): void {
    this.indentationByTrack.set(track, this.getIndentation(track) + 1);
  }

  private decrementIndentation(track: string): void {
    const newLevel = Math.max(0, this.getIndentation(track) - 1);
    this.indentationByTrack.set(track, newLevel);
  }

  /**
   * Main entry point for adding a new journal entry to the table.
   */
  addEntry(entry: JournalEntry) {
    const {row, cells} = this.createRow(entry);
    const typeCell = cells[3]!;
    const detailsCell = cells[4]!;
    if (entry.track === 'FrontEnd') {
      typeCell.classList.add('frontend-track');
    }

    this.formatTypeCell(entry, row, typeCell);
    this.formatDetailsCell(entry, detailsCell);
    this.insertRow(row, entry.timestamp.getTime());
  }

  /**
   * Creates the <tr> element and all <td> cells from the template.
   */
  private createRow(entry: JournalEntry) {
    const clone = (this.template.content.cloneNode(true) as DocumentFragment);
    const row = clone.children[0] as HTMLTableRowElement;
    row.dataset['timestamp'] = entry.timestamp.getTime().toString();
    row.dataset['taskId'] = entry.taskId.toString();
    row.dataset['type'] = entry.type.toString();
    row.dataset['track'] = entry.track.toString();

    const cells = clone.querySelectorAll('td');
    cells[0]!.textContent = entry.taskId === 0 ? '' : entry.taskId.toString();
    cells[1]!.textContent = entry.url;
    cells[2]!.textContent = entry.event;
    cells[5]!.textContent =
        new Date(entry.timestamp).toLocaleTimeString(undefined, {
          hour12: false,
          timeZoneName: 'short',
        });

    return {row, cells};
  }

 private formatMapDetails(detailsMap: Map<string, string>): string {
  return [...detailsMap]
    .filter(([_, value]) => value)
    .map(([key, value]) => `${key}: ${value}`)
    .join('\n');
  }

  private formatDetailsCell(entry: JournalEntry,
      detailsCell: HTMLTableCellElement ) {
    if (entry.event === 'GlicPerformActions' && entry.details['proto']) {
      const protobytes = entry.details['proto'];
      const link = document.createElement('a');
      link.textContent = 'Actions Proto';
      link.href = `https://protoshop.corp.google.com/embed?tabs=viewer,editor&type=chrome_intelligence_proto_features.Actions&protobytes=${
          protobytes}`;
      link.target = '_blank';
      detailsCell.appendChild(link);
    } else if (entry.screenshot) {
      const byteArray = new Uint8Array(entry.screenshot);
      const blob = new Blob([byteArray], { type: 'image/jpeg' });
      const blobUrl = URL.createObjectURL(blob);

      const img = document.createElement('img');
      img.src = blobUrl; // Use the Blob URL for the small image
      img.style.width = '200px';
      img.style.height = '100px';
      img.style.cursor = 'pointer'; // Indicate it's clickable

      img.onclick = () => {
          const newTab = window.open(blobUrl, '_blank');
          if (newTab) {
              newTab.focus();
          }
      };

      detailsCell.appendChild(img);
    } else {
      detailsCell.textContent = this.formatMapDetails(new Map(Object.entries(entry.details)));
    }
    detailsCell.classList.add('whitespace-pre');
    if ('error' in entry.details) {
      detailsCell.classList.add('error-cell');
    }
  }

  private formatTypeCell(
      entry: JournalEntry, row: HTMLTableRowElement,
      typeCell: HTMLTableCellElement) {
    const typeSpan = document.createElement('span');
    typeCell.classList.add('type-cell');

    const newSpanId = this.spanIdCounter++;
    row.dataset['spanId'] = newSpanId.toString();

    let indentLevel = 0;
    let parentIds: number[] = [];

    if (entry.type === 'Begin') {
      // 1. Get the current indentation level for this track (e.g., N)
      indentLevel = this.getIndentation(entry.track);
      parentIds = this.activeSpans.map(s => s.id);

      // 2. Render this 'Begin' span at level N
      typeCell.textContent = ' '.repeat(indentLevel * 2) + '+ ';
      typeCell.onclick = () => this.toggleSpan(row);
      this.spanState.set(newSpanId, {collapsed: false, parentIds});

      // 3. Add to stack and increment the map *for its future children* (to
      // N+1)
      this.activeSpans.push({id: newSpanId, track: entry.track, event: entry.event});
      this.incrementIndentation(entry.track);

      typeSpan.className = 'type-b';

    } else if (entry.type === 'End') {
      // This 'End' event closes the current span. Decrement the indent
      // level for this track *first* (from N+1 back to N).
      this.decrementIndentation(entry.track);

      // Get the new, correct indentation level (N)
      indentLevel = this.getIndentation(entry.track);

      // Get parent list *before* removing its own Begin span from the stack.
      // This correctly makes the End row a child of its Begin row.
      parentIds = this.activeSpans.map(s => s.id);

      // Find and remove the last active span *matching this track*.
      // Do NOT just pop(), as the top of stack might be from another track.
      let endedSpanIndex = -1;
      for (let i = this.activeSpans.length - 1; i >= 0; i--) {
        if (this.activeSpans[i]!.track === entry.track &&
            this.activeSpans[i]!.event.startsWith(entry.event)) {
          endedSpanIndex = i;
          break;
        }
      }
      if (endedSpanIndex > -1) {
        this.activeSpans.splice(endedSpanIndex, 1);
      }

      // Render this 'End' span at level N
      typeCell.textContent = ' '.repeat(indentLevel * 2) + '  ';
      this.spanState.set(newSpanId, {collapsed: false, parentIds});
      typeSpan.className = 'type-e';

    } else {
      indentLevel = this.getIndentation(entry.track);
      parentIds = this.activeSpans.map(s => s.id);

      typeCell.textContent = ' '.repeat(indentLevel * 2) + '  ';
      this.spanState.set(newSpanId, {collapsed: false, parentIds});
    }

    typeSpan.textContent = entry.type;
    typeCell.appendChild(typeSpan);
    typeCell.classList.add('whitespace-pre');
  }

  /**
   * Inserts the new row into the table, sorted by timestamp.
   */
  private insertRow(row: HTMLTableRowElement, newTimestamp: number) {
    const rows = this.table.rows;
    for (let i = rows.length - 1; i > 0; i--) {
      const existingTimestamp = Number(rows[i]!.dataset['timestamp']);
      if (newTimestamp >= existingTimestamp) {
        // Insert after this row
        rows[i]!.after(row);
        return;
      }
    }
    // If we reached the top or table is empty, append after header
    rows[0]!.after(row);
  }

  /**
   * Toggles the collapsed state of a span.
   */
  toggleSpan(row: HTMLTableRowElement) {
    const toggledSpanId = Number(row.dataset['spanId']);
    const state = this.spanState.get(toggledSpanId);
    if (!state) {
      return;
    }

    const toCollapse = !state.collapsed;
    state.collapsed = toCollapse;

    // Update the +/- icon on the clicked row.
    const typeCell = row.querySelector<HTMLTableCellElement>('.type-cell');
    const textNode = typeCell!.childNodes[0];
    textNode!.nodeValue = toCollapse ? textNode!.nodeValue!.replace('+', '-') :
                                       textNode!.nodeValue!.replace('-', '+');

    // Update visibility for all subsequent rows that are descendants.
    for (let i = row.rowIndex + 1; i < this.table.rows.length; i++) {
      const childRow = this.table.rows[i]!;
      const parentSpans = this.getParentIds(childRow);

      // If the row is not a descendant, we can stop iterating.
      if (!parentSpans.includes(toggledSpanId)) {
        break;
      }

      // If the child row is not on the same track, do not change its
      // visibility.
      if (childRow.dataset['track'] !== row.dataset['track']) {
        continue;
      }

      // This row is a descendant. It should be visible ONLY if *none* of its
      // parents (including the one just toggled) are collapsed.
      const isHidden = this.isAnyParentCollapsed(childRow);
      childRow.style.display = isHidden ? 'none' : '';

      // Add animation class if we are un-collapsing
      if (!isHidden && !toCollapse) {
        childRow.classList.add('newly-uncollapsed');
        setTimeout(() => {
          childRow.classList.remove('newly-uncollapsed');
        }, 2000);
      }
    }
  }


  /**
   * Displays all rows, respecting the current collapsed state of parent spans.
   */
  clearFilter() {
    getRequiredElement<HTMLInputElement>('task-id-filter').value = '';
    const rows = Array.from(this.table.rows);
    for (let i = 1; i < rows.length; i++) {
      const row = rows[i]!;
      // Show row only if none of its parents are collapsed
      row.style.display = this.isAnyParentCollapsed(row) ? 'none' : '';
    }
  }

  /**
   * Displays only rows matching a task ID, respecting collapsed state.
   */
  filterByTaskId() {
    const taskId =
        getRequiredElement<HTMLInputElement>('task-id-filter').value.trim();
    if (!taskId) {
      this.clearFilter();  // No filter text, just run a clear.
      return;
    }

    const rows = Array.from(this.table.rows);
    let firstIndex = -1;
    let lastIndex = -1;

    for (let i = 1; i < rows.length; i++) {
      if (rows[i]!.dataset['taskId'] === taskId) {
        if (firstIndex === -1) {
          firstIndex = i;
        }
        lastIndex = i;
      }
    }

    if (firstIndex === -1) {
      // No matches, hide everything
      for (let i = 1; i < rows.length; i++) {
        rows[i]!.style.display = 'none';
      }
      return;
    }

    // Show/hide all rows based on filter range AND collapse state
    for (let i = 1; i < rows.length; i++) {
      const row = rows[i]!;
      const inRange = (i >= firstIndex && i <= lastIndex);

      if (!inRange) {
        row.style.display = 'none';
      } else {
        // It's in the task range. Show it, but only if its parents aren't
        // collapsed.
        row.style.display = this.isAnyParentCollapsed(row) ? 'none' : '';
      }
    }
  }
}


function startLogging() {
  BrowserProxy.getInstance().handler.startLogging();
  getRequiredElement('start-logging').style.display = 'none';
  getRequiredElement('stop-logging').style.display = 'inline-block';
}

function stopLogging() {
  BrowserProxy.getInstance().handler.stopLogging();
  getRequiredElement('start-logging').style.display = 'inline-block';
  getRequiredElement('stop-logging').style.display = 'none';
}


window.onload = function() {
  const proxy = BrowserProxy.getInstance();
  const log = new ActorEventLog();

  // Route journal entries to the class instance
  proxy.callbackRouter.journalEntryAdded.addListener(
      (entry: JournalEntry) => log.addEntry(entry));

  // Hook up buttons
  getRequiredElement('start-logging').onclick = startLogging;
  getRequiredElement('stop-logging').onclick = stopLogging;
  getRequiredElement('filter-by-task-id').onclick = () => log.filterByTaskId();
  getRequiredElement('clear-filter').onclick = () => log.clearFilter();
};
