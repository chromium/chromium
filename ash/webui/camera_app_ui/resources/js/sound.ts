// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WaitableEvent} from './waitable_event.js';

/**
 * Audio element status.
 */
export enum Status {
  LOADING = 'loading',
  PAUSED = 'paused',
  PLAYING = 'playing',
}

/**
 * Map of audio elements and their status.
 */
const elementsStatus = new Map<HTMLAudioElement, Status>();

/**
 * Plays a sound.
 *
 * @param el Audio element to play.
 * @return Promise which will be resolved once the sound is stopped. The
 *     resolved value will be true if it is ended. Otherwise, it is just paused
 *     due to cancellation.
 */
export async function play(el: HTMLAudioElement): Promise<boolean> {
  await cancel(el).wait();

  el.currentTime = 0;
  elementsStatus.set(el, Status.LOADING);
  await el.play();
  elementsStatus.set(el, Status.PLAYING);

  const audioStopped = new WaitableEvent<boolean>();
  const events = ['ended', 'pause'];
  function onAudioStopped() {
    elementsStatus.set(el, Status.PAUSED);

    // TODO(b/223338160): A temporary workaround to avoid shutter sound being
    // recorded.
    const ended = el.ended;
    setTimeout(() => audioStopped.signal(ended), 200);
    for (const event of events) {
      el.removeEventListener(event, onAudioStopped);
    }
  }
  for (const event of events) {
    el.addEventListener(event, onAudioStopped);
  }
  return audioStopped.wait();
}

/**
 * Cancel a sound from playing. If the sound is loading, cancel it right after
 * it start playing. If the sound is paused, do nothing.
 *
 * @param el Audio element to cancel.
 */
export function cancel(el: HTMLAudioElement): WaitableEvent {
  // We can only pause the element which is currently playing.
  // (Please refer to https://goo.gl/LdLk22)
  // TODO(pihsun): Check if we can just call el.pause() and handle the
  // AbortError in play instead.
  const status = elementsStatus.get(el);
  const canceled = new WaitableEvent();
  if (status === Status.PLAYING) {
    el.pause();
    canceled.signal();
  } else if (status === Status.LOADING) {
    function onPlaying() {
      el.pause();
      canceled.signal();
    }
    el.addEventListener('playing', onPlaying, {once: true});
  } else {
    canceled.signal();
  }
  return canceled;
}
