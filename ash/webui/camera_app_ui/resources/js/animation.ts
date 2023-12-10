// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists, assertInstanceof} from './assert.js';
import {AsyncJobInfo, AsyncJobQueue} from './async_job_queue.js';

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
function getAnimations(el: HTMLElement): Animation[] {
  if (el.shadowRoot !== null) {
    // The element is a custom web component, assuming that we want to wait for
    // all inner animations to settle down when applying animation to the
    // element.
    return el.shadowRoot.getAnimations();
  }
  return el.getAnimations({subtree: true})
      .filter((a) => assertInstanceof(a.effect, KeyframeEffect).target === el);
}

/**
 * Cancels the running animation on the element, if any.
 *
 * @param el Target element to cancel animation.
 */
export function cancel(el: HTMLElement): void {
  for (const a of getAnimations(el)) {
    a.cancel();
  }
}

/**
 * Animates the target element once by applying the "animate" class. If the
 * animation is already running, the previous one would be cancelled first.
 *
 * @param el Target element to apply "animate" class.
 * @param changeElement Function to change the target element before animation.
 */
export function play(
    el: HTMLElement, changeElement?: () => void): AsyncJobInfo {
  cancel(el);
  const queue = getQueueFor(el);
  return queue.push(async () => {
    void el.offsetWidth;  // Force repaint before applying the animation.
    if (changeElement !== undefined) {
      changeElement();
    }
    el.classList.add('animate');
    await Promise.allSettled(getAnimations(el).map((a) => a.finished));
    el.classList.remove('animate');
  });
}
