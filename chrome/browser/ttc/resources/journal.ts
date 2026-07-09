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
  content: string|null;
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

  // Concatenates the supplied input and output transcriptions to the latest
  // Turn's transcriptions if the latest turn is still in progress. If the
  // latest turn is completed or no turn yet exists, a new Turn entry is first
  // created.
  updateCurrentTurn(input?: string, output?: string) {
    const turn = this.getOrCreateLastTurn();
    if (input) {
      turn.inputTranscript += input;
    }
    if (output) {
      turn.outputTranscript += output;
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
