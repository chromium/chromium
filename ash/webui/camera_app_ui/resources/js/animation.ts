// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {AsyncJobQueue} from './async_job_queue.js';

const jobQueueMap = new Map<HTMLElement, AsyncJobQueue>();

/**
 * Gets the animation job queue for the element.
 */
function getQueueFor(el: HTMLElement): AsyncJobQueue {
  if (!jobQueueMap.has(el)) {
    jobQueueMap.set(el, new AsyncJobQueue());
  }
  return jobQueueMap.get(el);
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
 * @param param |el| is the target element to cancel animation. |onChild|
 *     specifies whether the cancelled animation is applied to all subtree
 *     children, false by default.
 * @return Promise resolved when the animation is cancelled.
 */
async function doCancel({el, onChild}: {el: HTMLElement, onChild: boolean}):
    Promise<void> {
  getAnimations({el, onChild}).forEach((a) => a.cancel());
  await getQueueFor(el).flush();
}

/**
 * Cancels the running animation on the element, if any.
 * @return Promise resolved when the animation is cancelled.
 */
export async function cancel(el: HTMLElement): Promise<void> {
  return doCancel({el, onChild: false});
}

/**
 * Cancels all running animation on children of the element, if any.
 * @return Promise resolved when all animation is cancelled.
 */
export async function cancelOnChild(el: HTMLElement): Promise<void> {
  return doCancel({el, onChild: true});
}

/**
 * Animates the target element once by applying the "animate" class. If the
 * animation is already running, the previous one would be cancelled first.
 * @param param |el| is the target element to apply "animate" class. |onChild|
 *     specifies whether the animation is applied to all subtree children.
 * @return Promise resolved when the animation is settled.
 */
function doPlay({el, onChild}: {el: HTMLElement, onChild: boolean}):
    Promise<void> {
  doCancel({el, onChild});
  const queue = getQueueFor(el);
  const job = async () => {
    /**
     * Force repaint before applying the animation.
     */
    el.offsetWidth;
    el.classList.add('animate');
    await Promise.allSettled(
        getAnimations({el, onChild}).map((a) => a.finished));
    el.classList.remove('animate');
  };
  return queue.push(job);
}

/**
 * Sets "animate" class on the element and waits for its animation settled.
 * @return Promise resolved when the animation is settled.
 */
export function play(el: HTMLElement): Promise<void> {
  return doPlay({el, onChild: false});
}

/**
 * Sets "animate" class on the element and waits for its child's animation
 * settled.
 * @return Promise resolved when the child's animation is settled.
 */
export function playOnChild(el: HTMLElement): Promise<void> {
  return doPlay({el, onChild: true});
}
