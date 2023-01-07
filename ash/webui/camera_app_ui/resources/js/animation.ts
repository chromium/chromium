// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists, assertInstanceof} from './assert.js';
import {AsyncJobQueue} from './async_job_queue.js';

const jobQueueMap = new Map<HTMLElement, AsyncJobQueue>();

/**
 * Gets the animation job queue for the element.
 */
function getQueueFor(el: HTMLElement): AsyncJobQueue {
  if (!jobQueueMap.has(el)) {
    jobQueueMap.set(el, new AsyncJobQueue());
  }
  // We just set the map value if it's not in the jobQueueMap already.
  return assertExists(jobQueueMap.get(el));
}

/**
 * Gets all the animations running or pending on the element and its
 * pseudo-elements.
 */
function getAnimations({el, onChild}: {el: HTMLElement, onChild: boolean}):
    Animation[] {
  return el.getAnimations({subtree: true})
      .filter(
          (a) => onChild ||
              assertInstanceof(a.effect, KeyframeEffect).target === el);
}

/**
 * @param params Cancel parameters.
 * @param params.el Target element to cancel animation.
 * @param params.onChild Specifies whether the cancelled animation is applied to
 *     all subtree children, false by default.
 * @return Promise resolved when the animation is cancelled.
 */
async function doCancel({el, onChild}: {el: HTMLElement, onChild: boolean}):
    Promise<void> {
  for (const a of getAnimations({el, onChild})) {
    a.cancel();
  }
  await getQueueFor(el).flush();
}

/**
 * Cancels the running animation on the element, if any.
 *
 * @return Promise resolved when the animation is cancelled.
 */
export async function cancel(el: HTMLElement): Promise<void> {
  return doCancel({el, onChild: false});
}

/**
 * Cancels all running animation on children of the element, if any.
 *
 * @return Promise resolved when all animation is cancelled.
 */
export async function cancelOnChild(el: HTMLElement): Promise<void> {
  return doCancel({el, onChild: true});
}

/**
 * Animates the target element once by applying the "animate" class. If the
 * animation is already running, the previous one would be cancelled first.
 *
 * @param params Play parameters.
 * @param params.el Target element to apply "animate" class.
 * @param params.changeElement Function to change the target element
 *     before animation.
 * @param params.onChild Specifies whether the animation is applied to all
 *     subtree children.
 * @return Promise resolved when the animation is settled.
 */
async function doPlay(
    {el, onChild, changeElement}:
        {el: HTMLElement, onChild: boolean, changeElement?: () => void}):
    Promise<void> {
  await doCancel({el, onChild});
  const queue = getQueueFor(el);
  async function job() {
    void el.offsetWidth;  // Force repaint before applying the animation.
    if (changeElement) {
      changeElement();
    }
    el.classList.add('animate');
    await Promise.allSettled(
        getAnimations({el, onChild}).map((a) => a.finished));
    el.classList.remove('animate');
  }
  await queue.push(job);
}

/**
 * Sets "animate" class on the element and waits for its animation settled.
 *
 * @return Promise resolved when the animation is settled.
 */
export function play(
    el: HTMLElement, changeElement?: () => void): Promise<void> {
  return doPlay({el, onChild: false, changeElement});
}

/**
 * Sets "animate" class on the element and waits for its child's animation
 * settled.
 *
 * @return Promise resolved when the child's animation is settled.
 */
export function playOnChild(
    el: HTMLElement, changeElement?: () => void): Promise<void> {
  return doPlay({el, onChild: true, changeElement});
}
