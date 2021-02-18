// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @enum {number}
 * The type of entity a given command represents. Must stay in sync with
 * commander::CommandItem::Entity.
 */
export const Entity = {
  COMMAND: 0,
  BOOKMARK: 1,
  TAB: 2,
  WINDOW: 3,
  GROUP: 4,
};

/**
 * @enum {number}
 * The action that should be taken when the view model is received by the view.
 * Must stay in sync with commander::CommanderViewModel::Action. "CLOSE" is
 * included for completeness, but should be handled before the view model
 * reaches the WebUI layer.
 */
export const Action = {
  DISPLAY_RESULTS: 0,
  CLOSE: 1,
  PROMPT: 2,
};

/**
 * View model for a result option.
 * Corresponds to commander::CommandItemViewModel.
 * @typedef {{
 *   title : string,
 *   annotation : (string|undefined),
 *   entity : Entity,
 *   matchedRanges : !Array<!Array<number>>,
 * }}
 */
export let Option;

/**
 * View model for a result set.
 * Corresponds to commander::CommanderViewModel.
 * @typedef {{
 *   action : Action,
 *   resultSetId : number,
 *   options : ?Array<Option>,
 *   promptText : (string|undefined),
 * }}
 */
export let ViewModel;
