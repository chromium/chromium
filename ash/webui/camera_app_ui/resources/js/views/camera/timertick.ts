// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../../animation.js';
import * as dom from '../../dom.js';
import {play} from '../../sound.js';
import * as state from '../../state.js';
import {CanceledError} from '../../type.js';

/**
 * Handler to cancel the active running timer-ticks.
 */
let doCancel: (() => void)|null = null;

/**
 * Starts timer ticking if applicable.
 *
 * @return Promise for the operation.
 */
export async function start(): Promise<void> {
  doCancel = null;

  const isTimerOptionShown =
      getComputedStyle(dom.get('#open-timer-panel', HTMLElement)).display !==
      'none';
  if (!state.get(state.State.TIMER) || !isTimerOptionShown) {
    return Promise.resolve();
  }

  state.set(state.State.TIMER_TICK, true);
  try {
    await new Promise<void>((resolve, reject) => {
      let tickTimeout: number|null = null;
      const tickMsg = dom.get('#timer-tick-msg', HTMLElement);
      doCancel = () => {
        if (tickTimeout !== null) {
          clearTimeout(tickTimeout);
          tickTimeout = null;
        }
        animate.cancel(tickMsg);
        reject(new CanceledError('Timer tick is canceled'));
      };

      let tickCounter = state.get(state.State.TIMER_10SEC) ? 10 : 3;
      const sounds = new Map([
        [1, 'tickFinal'],
        [2, 'tickIncrement'],
        [3, 'tickIncrement'],
        [tickCounter, 'tickStart'],
      ] as const);

      function onTimerTick() {
        if (tickCounter === 0) {
          resolve();
        } else {
          const sound = sounds.get(tickCounter);
          if (sound !== undefined) {
            play(sound);
          }
          tickMsg.textContent = tickCounter + '';
          animate.play(tickMsg);
          tickTimeout = setTimeout(onTimerTick, 1000);
          tickCounter--;
        }
      }
      // First tick immediately in the next message loop cycle.
      tickTimeout = setTimeout(onTimerTick, 0);
    });
  } finally {
    state.set(state.State.TIMER_TICK, false);
  }
}

/**
 * Cancels active timer ticking if applicable.
 */
export function cancel(): void {
  doCancel?.();
  doCancel = null;
}
