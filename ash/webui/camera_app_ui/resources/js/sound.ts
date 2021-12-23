// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WaitableEvent} from './waitable_event.js';

/**
 * Audio element status.
 */
export enum Status {
  LOADING = 'loading',
  PLAYING = 'playing',
  PAUSED = 'paused',
}

/**
 * Map of audio elements and their status.
 */
const elementsStatus = new Map<HTMLAudioElement, Status>();

/**
 * Plays a sound.
 * @param el Audio element to play.
 * @return Promise which will be resolved once the sound is stopped. The
 *     resolved value will be true if it is ended. Otherwise, it is just paused
 *     due to cancenlation.
 */
export async function play(el: HTMLAudioElement): Promise<boolean> {
  await cancel(el);

  el.currentTime = 0;
  elementsStatus.set(el, Status.LOADING);
  await el.play();
  elementsStatus.set(el, Status.PLAYING);

  const audioStopped = new WaitableEvent<boolean>();
  const events = ['ended', 'pause'];
  const onAudioStopped = () => {
    elementsStatus.set(el, Status.PAUSED);
    audioStopped.signal(el.ended);
    for (const event of events) {
      el.removeEventListener(event, onAudioStopped);
    }
  };
  for (const event of events) {
    el.addEventListener(event, onAudioStopped);
  }
  return audioStopped.wait();
}

/**
 * Cancel a sound from playing. If the sound is loading, cancel it right after
 * it start playing. If the sound is paused, do nothing.
 * @param el Audio element to cancel.
 */
export async function cancel(el: HTMLAudioElement): Promise<void> {
  // We can only pause the element which is currently playing.
  // (Please refer to https://goo.gl/LdLk22)
  const status = elementsStatus.get(el);
  if (status === Status.PLAYING) {
    el.pause();
  } else if (status === Status.LOADING) {
    const canceled = new WaitableEvent();
    const onPlaying = () => {
      el.pause();
      canceled.signal();
    };
    el.addEventListener('playing', onPlaying, {once: true});
    await canceled.wait();
  }
}
