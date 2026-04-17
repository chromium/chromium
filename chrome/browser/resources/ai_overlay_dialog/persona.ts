// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {ConversationConfig} from './conversation.js';
import type {PersistedPageContext, Turn} from './journal.js';

/**
 * Processes the template for conditional blocks in the format:
 *
 * "?${expr}[string1]{else}[string2]{}?" or "?${expr}[string1]{}?".
 *
 * If data[expr] evaluates to true the entire substring is replaced by string1.
 * Otherwise by string2 in the case of {else} or removed entirely otherwise.
 */
export function processConditionals(
    template: string, data: Record<string, unknown>): string {
  let result = template;
  const openingRegex = /\?\$\{([a-zA-Z0-9_]+)\}\[/;

  while (true) {
    const match = result.match(openingRegex);
    if (!match) {
      break;
    }

    const openingIndex = match.index!;
    assert(match[1]);
    const expr = match[1];
    const openingEnd = openingIndex + match[0].length;

    if (!(expr in data)) {
      throw new Error(`Key '${expr}' not found in data`);
    }

    const value = !!data[expr];
    const terminatingStr = ']{}?';
    const elseStr = ']{else}[';

    let nextTerminating = result.indexOf(terminatingStr, openingEnd);
    let nextElse = result.indexOf(elseStr, openingEnd);

    if (nextTerminating === -1 && nextElse === -1) {
      throw new Error('No terminating or else directive found after opening');
    } else if (nextTerminating === -1) {
      nextTerminating = Infinity;
    } else if (nextElse === -1) {
      nextElse = Infinity;
    }

    let nextDirectiveIndex: number;
    let isElse: boolean;

    if (nextTerminating < nextElse) {
      nextDirectiveIndex = nextTerminating;
      isElse = false;
    } else {
      nextDirectiveIndex = nextElse;
      isElse = true;
    }

    const string1 = result.substring(openingEnd, nextDirectiveIndex);
    let string2 = '';
    let totalEndIndex: number;

    if (!isElse) {
      totalEndIndex = nextDirectiveIndex + terminatingStr.length;
    } else {
      const elseEnd = nextDirectiveIndex + elseStr.length;
      if (nextTerminating === -1) {
        throw new Error('No terminating directive found after else');
      }

      const secondElse = result.indexOf(elseStr, elseEnd);
      if (secondElse !== -1 && secondElse < nextTerminating) {
        throw new Error(
            'Multiple else directives found for a single conditional');
      }

      string2 = result.substring(elseEnd, nextTerminating);
      totalEndIndex = nextTerminating + terminatingStr.length;
    }

    const replacement = value ? string1 : string2;
    result = result.substring(0, openingIndex) + replacement +
        result.substring(totalEndIndex);
  }

  return result;
}

/**
 * Processes numbering tokens in the format "#{n}" where n is a group number.
 * Each group starts at 1 and increments each time it's encountered.
 */
export function processNumbering(template: string): string {
  const groups = new Map<string, number>();
  return template.replace(/#\{(\d+)\}/g, (_match, group) => {
    const current = groups.get(group) || 1;
    groups.set(group, current + 1);
    return current.toString();
  });
}

export function processTemplate(
    template: string, data: Record<string, unknown>): string {
  const withConditionals = processConditionals(template, data);
  const withNumbering = processNumbering(withConditionals);
  return withNumbering.replace(/\$\{([a-zA-Z0-9_]+)\}/g, (match, key) => {
    return key in data ? String(data[key]) : match;
  });
}

export function formatTranscript(turns: Turn[], personaName: string): string {
  return turns.filter(turn => turn.isComplete)
      .map(turn => {
        return `User: ${turn.inputTranscript}\n${personaName}: ${
            turn.outputTranscript}`;
      })
      .join('\n');
}

export function formatPageVisitHistory(pages: PersistedPageContext[]): string {
  return pages
      .map((page, index) => {
        return `${index + 1}. [${page.title || 'Untitled'}](${page.url})`;
      })
      .join('\n');
}

export function buildSystemInstruction(
    config: ConversationConfig, url: string, title: string, pageContent: string,
    transcript: string, pageHistory: string): string {
  const data: Record<string, unknown> = {
    persona: config.persona.persona,
    title: title,
    url: url,
    pageHistory: pageHistory,
    conversation: transcript,
    nameList: config.persona.nicknames,
    isYT: false,
    isQuizlet: false,
    compressed: false,
    tabLines: '',
    truncatedPageContent: pageContent,
  };

  const template = config.system_instruction;
  return processTemplate(template, data);
}
