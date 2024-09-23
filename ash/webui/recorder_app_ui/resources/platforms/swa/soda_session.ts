// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SodaEvent, SodaSession} from '../../core/soda/types.js';
import {
  Observer,
  ObserverList,
  Unsubscribe,
} from '../../core/utils/observer_list.js';
import {clamp} from '../../core/utils/utils.js';
import {WaitableEvent} from '../../core/utils/waitable_event.js';
import {SpeechRecognizerEvent} from '../../mojom/soda.mojom-webui.js';

import {SodaClientInterface, SodaRecognizerRemote} from './types.js';

export class MojoSodaSession implements SodaSession, SodaClientInterface {
  private readonly observers = new ObserverList<SodaEvent>();

  private readonly startEvent = new WaitableEvent();

  private readonly stopEvent = new WaitableEvent();

  constructor(private readonly recognizer: SodaRecognizerRemote) {}

  async start(): Promise<void> {
    this.recognizer.start();
    await this.startEvent.wait();
  }

  addAudio(samples: Float32Array): void {
    // Scale to int16 array.
    const scaledSamples = new Int16Array(
      samples.map((v) => clamp(Math.floor(v * 32768), -32768, 32767)),
    );
    // The addAudio argument is a uint8 array for the bytes of the int16 array.
    const toSend = Array.from(new Uint8Array(scaledSamples.buffer));
    this.recognizer.addAudio(toSend);
  }

  async stop(): Promise<void> {
    this.recognizer.markDone();
    await this.stopEvent.wait();
  }

  subscribeEvent(observer: Observer<SodaEvent>): Unsubscribe {
    return this.observers.subscribe(observer);
  }

  onStart(): void {
    this.startEvent.signal();
  }

  onStop(): void {
    this.stopEvent.signal();
  }

  onSpeechRecognizerEvent(event: SpeechRecognizerEvent): void {
    if (event.partialResult !== undefined) {
      this.observers.notify({partialResult: event.partialResult});
    } else if (event.finalResult !== undefined) {
      this.observers.notify({finalResult: event.finalResult});
    } else if (event.labelCorrectionEvent !== undefined) {
      this.observers.notify({
        labelCorrectionEvent: event.labelCorrectionEvent,
      });
    }
  }
}
