// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from './assert.js';
import {AsyncJobInfo, AsyncJobQueue} from './async_job_queue.js';
import {expandPath} from './util.js';
import {WaitableEvent} from './waitable_event.js';

const SOUND_FILES = new Map([
  ['recordEnd', 'record_end.ogg'],
  ['recordPause', 'record_pause.ogg'],
  ['recordStart', 'record_start.ogg'],
  ['shutter', 'shutter.ogg'],
  ['tickFinal', 'tick_final.ogg'],
  ['tickIncrement', 'tick_inc.ogg'],
  ['tickStart', 'tick_start.ogg'],
] as const);

type SoundKey = typeof SOUND_FILES extends Map<infer K, unknown>? K : never;

interface SoundInfo {
  element: HTMLAudioElement;
  queue: AsyncJobQueue;
}

let soundInfoMap: Map<SoundKey, SoundInfo>|null = null;

/**
 * Preloads all sounds used in CCA.
 *
 * This should only be called once in main.ts.
 */
export function preloadSounds(): void {
  assert(soundInfoMap === null, 'preloadSounds should only be called once');
  soundInfoMap = new Map();
  for (const [key, filename] of SOUND_FILES.entries()) {
    soundInfoMap.set(key, {
      element: new Audio(expandPath(`/sounds/${filename}`)),
      queue: new AsyncJobQueue('keepLatest'),
    });
  }
}

function getSoundInfo(key: SoundKey): SoundInfo {
  assert(soundInfoMap !== null, 'preloadSounds should be called before this');
  return assertExists(soundInfoMap.get(key));
}

/**
 * Plays a sound.
 *
 * @param key Sound to play.
 * @return The async job which will be resolved once the sound is stopped.
 */
export function play(key: SoundKey): AsyncJobInfo {
  cancel(key);

  const {element: el, queue} = getSoundInfo(key);
  return queue.push(async () => {
    el.currentTime = 0;
    try {
      await el.play();
    } catch (e) {
      if (e instanceof DOMException && e.name === 'AbortError') {
        // playing is cancelled.
        return;
      }
      throw e;
    }

    const audioStopped = new WaitableEvent();
    const events = ['ended', 'pause'];
    function onAudioStopped() {
      audioStopped.signal();
      for (const event of events) {
        el.removeEventListener(event, onAudioStopped);
      }
    }
    for (const event of events) {
      el.addEventListener(event, onAudioStopped);
    }
    await audioStopped.wait();
  });
}

/**
 * Cancel a sound from playing.
 *
 * @param key Sound to cancel.
 */
export function cancel(key: SoundKey): void {
  getSoundInfo(key).element.pause();
}
