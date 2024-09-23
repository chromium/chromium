// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface for the central output class for ChromeVox.
 */
import {CursorRange} from '/common/cursors/range.js';

import {Spannable} from '../../common/spannable.js';

import {OutputFormatLogger} from './output_logger.js';
import {OutputAction, OutputEventType} from './output_types.js';

import AutomationNode = chrome.automation.AutomationNode;

export interface AnnotationOptions {
  annotation: any[];
  isUnique?: boolean;
}

export interface RenderArgs {
  suppressStartEndAncestry?: boolean;
  preferStart?: boolean;
  preferEnd?: boolean;
}

// These functions are public only to output/ classes.
export abstract class OutputInterface {
  /** Appends output to the |buff|. */
  abstract append(
      buff: Spannable[], value: string|Spannable,
      options?: AnnotationOptions): void;
  abstract assignLocaleAndAppend(
      text: string, contextNode: AutomationNode, buff: Spannable[],
      options?: AnnotationOptions): void;

  /** Find the earcon for a given node (including ancestry). */
  abstract findEarcon(node: AutomationNode, prevNode?: AutomationNode):
      OutputAction|undefined;

  abstract formatNode(
      node: AutomationNode, prevNode: AutomationNode, type: OutputEventType,
      buff: Spannable[], formatLog: OutputFormatLogger): void;

  /**
   * Renders the given range using optional context previous range and event
   * type.
   */
  abstract render(
      range: CursorRange, prevRange: CursorRange|undefined,
      type: OutputEventType, buff: Spannable[], formatLog: OutputFormatLogger,
      args?: RenderArgs): void;
  abstract shouldSuppress(token: string): boolean;

  abstract get useAuralStyle(): boolean;
  abstract get formatAsBraille(): boolean;
  abstract get formatAsSpeech(): boolean;
}
