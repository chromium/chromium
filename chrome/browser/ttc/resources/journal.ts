// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageContextChangeType} from './page_context_manager.js';
import type {PageContextChangeEvent, PageContextManager} from './page_context_manager.js';

export enum JournalEntryType {
  TURN = 'turn',
  NEW_PAGE = 'new_page',
}

export interface Turn {
  /* User's transcript this turn */
  inputTranscript: string;
  /* Model's transcript this turn */
  outputTranscript: string;
  /* Whether the turn has been completed or is still in-progress */
  isComplete: boolean;
}

export interface PersistedPageContext {
  url: string;
  title: string|null;
  content: unknown|null;
}

export enum ChunkAccumulationMode {
  /* Replaces existing text (e.g. for accumulating snapshot recognizers). */
  REPLACE = 'replace',
  /* Appends to existing text (e.g. for delta chunk streams). */
  APPEND = 'append',
}

export interface UpdateTurnOptions {
  inputMode?: ChunkAccumulationMode;
  outputMode?: ChunkAccumulationMode;
}

export interface JournalEntry {
  timestamp: number;
  type: JournalEntryType;
  data: Turn|PersistedPageContext;
}

/**
 * Journal is a persistent session journal for the conversation. It registers
 * entries for updates to the page context as well as transcription in session
 * turns.
 */
export class Journal {
  private readonly entries: JournalEntry[] = [];

  constructor(pageContextManager: PageContextManager) {
    // Note: The initial page context entry is handled by the listener as the
    // Conversation constructor will call createNewPageContext after this
    // constructor finishes.
    pageContextManager.registerListener(
        (event) => this.onPageContextChange(event));
  }

  getEntries(): JournalEntry[] {
    return this.entries;
  }

  getTurnEntries(): Turn[] {
    return this.entries.filter(e => e.type === JournalEntryType.TURN)
        .map(e => e.data as Turn);
  }

  getPageVisitEntries(): PersistedPageContext[] {
    return this.entries.filter(e => e.type === JournalEntryType.NEW_PAGE)
        .map(e => e.data as PersistedPageContext);
  }

  /**
   * Updates the supplied input and output transcriptions on the latest Turn.
   * If the latest turn is completed or no turn yet exists, a new Turn entry is
   * first created.
   *
   * Note on chunking behavior (`options`):
   * Different speech/streaming APIs yield text differently:
   * - Local/client speech recognizers (like Web Speech or Gemini live input)
   *   often send accumulating full snapshots ("hello" -> "hello world"),
   * requiring `ChunkAccumulationMode.REPLACE` (default for `input`).
   * - Model response streams (like Gemini Live output) typically yield delta
   *   chunks ("hello" -> " world" -> "!!"), requiring
   *   `ChunkAccumulationMode.APPEND` (default for `output`).
   */
  updateCurrentTurn(
      input?: string, output?: string, options: UpdateTurnOptions = {
        inputMode: ChunkAccumulationMode.REPLACE,
        outputMode: ChunkAccumulationMode.APPEND,
      }) {
    const turn = this.getOrCreateLastTurn();
    if (input !== undefined) {
      if (options.inputMode === ChunkAccumulationMode.APPEND) {
        turn.inputTranscript += input;
      } else {
        turn.inputTranscript = input;
      }
    }
    if (output !== undefined) {
      if (options.outputMode === ChunkAccumulationMode.REPLACE) {
        turn.outputTranscript = output;
      } else {
        turn.outputTranscript += output;
      }
    }
  }

  // Completes the most recent turn entry. If there is none or the latest turn
  // entry is already completed this will create and complete a new turn entry.
  completeTurn() {
    this.getOrCreateLastTurn().isComplete = true;
  }

  private getOrCreateLastTurn(): Turn {
    let lastTurnEntry =
        this.entries.findLast(entry => entry.type === JournalEntryType.TURN);

    if (!lastTurnEntry || (lastTurnEntry.data as Turn).isComplete) {
      lastTurnEntry = {
        timestamp: Date.now(),
        type: JournalEntryType.TURN,
        data: {
          inputTranscript: '',
          outputTranscript: '',
          isComplete: false,
        } as Turn,
      };
      this.entries.push(lastTurnEntry);
    }

    return lastTurnEntry.data as Turn;
  }

  private getLastPageEntry(): PersistedPageContext|undefined {
    const lastPageEntry = this.entries.findLast(
        entry => entry.type === JournalEntryType.NEW_PAGE);
    return lastPageEntry?.data as PersistedPageContext;
  }

  private onPageContextChange(event: PageContextChangeEvent) {
    if (event.type === PageContextChangeType.NEW_PAGE) {
      this.entries.push({
        timestamp: Date.now(),
        type: JournalEntryType.NEW_PAGE,
        data: {
          url: event.newContext.url,
          title: event.newContext.title,
          content: event.newContext.content,
        },
      });
    } else if (event.type === PageContextChangeType.UPDATE_CURRENT) {
      const pageData = this.getLastPageEntry();
      if (pageData) {
        pageData.title = event.newContext.title;
        pageData.content = event.newContext.content;
      }
    }
  }
}
