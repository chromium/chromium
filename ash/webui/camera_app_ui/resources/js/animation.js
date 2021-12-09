// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from './assert.js';
import {AsyncJobQueue} from './async_job_queue.js';

/**
 * @type {!Map<!HTMLElement, !AsyncJobQueue>}
 */
const jobQueueMap = new Map();

/**
 * Gets the animation job queue for the element.
 * @param {!HTMLElement} el
 * @return {!AsyncJobQueue}
 */
function getQueueFor(el) {
  if (!jobQueueMap.has(el)) {
    jobQueueMap.set(el, new AsyncJobQueue());
  }
  return jobQueueMap.get(el);
}

/**
 * Gets all the animations running or pending on the element and its
 * pseudo-elements.
 * TODO(b/176879728): Remove @suppress once we fix the getAnimations() extern
 * in upstream Closure compiler.
 * @suppress {checkTypes}
 * @param {{el: !HTMLElement, onChild: boolean}} param
 * @return {!Array<!Animation>}
 */
function getAnimations({el, onChild}) {
  return el.getAnimations({subtree: true})
      .filter(
          (a) => onChild ||
              assertInstanceof(a.effect, KeyframeEffect).target === el);
}

/**
 * @param {{el: !HTMLElement, onChild: boolean}} param |el| is the
 *     target element to cancel animation. |onChild| specifies whether the
 *     cancelled animation is applied to all subtree children, false by default.
 * @return {!Promise} Promise resolved when the animation is cancelled.
 */
async function doCancel({el, onChild}) {
  getAnimations({el, onChild}).forEach((a) => a.cancel());
  await getQueueFor(el).flush();
}

/**
 * Cancels the running animation on the element, if any.
 * @param {!HTMLElement} el
 * @return {!Promise} Promise resolved when the animation is cancelled.
 */
export async function cancel(el) {
  return doCancel({el, onChild: false});
}

/**
 * Cancels all running animation on children of the element, if any.
 * @param {!HTMLElement} el
 * @return {!Promise} Promise resolved when all animation is cancelled.
 */
export async function cancelOnChild(el) {
  return doCancel({el, onChild: true});
}

/**
 * Animates the target element once by applying the "animate" class. If the
 * animation is already running, the previous one would be cancelled first.
 * @param {{el: !HTMLElement, onChild: boolean}} param |el| is the
 *     target element to apply "animate" class. |onChild| specifies whether the
 *     animation is applied to all subtree children.
 * @return {!Promise} Promise resolved when the animation is settled.
 */
function doPlay({el, onChild}) {
  doCancel({el, onChild});
  const queue = getQueueFor(el);
  const job = async () => {
    /**
     * Force repaint before applying the animation.
     * @suppress {suspiciousCode}
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
 * @param {!HTMLElement} el
 * @return {!Promise} Promise resolved when the animation is settled.
 */
export function play(el) {
  return doPlay({el, onChild: false});
}

/**
 * Sets "animate" class on the element and waits for its child's animation
 * settled.
 * @param {!HTMLElement} el
 * @return {!Promise} Promise resolved when the child's animation is settled.
 */
export function playOnChild(el) {
  return doPlay({el, onChild: true});
}
