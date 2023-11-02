// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * The type of entity a given command represents. Must stay in sync with
 * commander::CommandItem::Entity.
 */
export enum Entity {
  COMMAND = 0,
  BOOKMARK = 1,
  TAB = 2,
  WINDOW = 3,
  GROUP = 4,
}

/**
 * The action that should be taken when the view model is received by the view.
 * Must stay in sync with commander::CommanderViewModel::Action. "CLOSE" is
 * included for completeness, but should be handled before the view model
 * reaches the WebUI layer.
 */
export enum Action {
  DISPLAY_RESULTS = 0,
  CLOSE = 1,
  PROMPT = 2,
}

// TODO(lgrey): Convert Option and ViewModel from class to type when tests
// are in TypeScript.
/**
 * View model for a result option.
 * Corresponds to commander::CommandItemViewModel.
 */
export class Option {
  title: string;
  annotation?: string;
  entity: Entity;
  matchedRanges: number[][];
}

/**
 * View model for a result set.
 * Corresponds to commander::CommanderViewModel.
 */
export class ViewModel {
  action: Action;
  resultSetId: number;
  options?: Option[];
  promptText?: string;
}
