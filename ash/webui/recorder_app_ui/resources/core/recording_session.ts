// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  POWER_SCALE_FACTOR,
  SAMPLE_RATE,
  SAMPLES_PER_SLICE,
} from './audio_constants.js';
import {PlatformHandler, SodaSession} from './platform_handler.js';
import {computed, signal} from './reactive/signal.js';
import {SodaEventTransformer, TextToken} from './soda/soda.js';
import {assertExhaustive} from './utils/assert.js';
import {Unsubscribe} from './utils/observer_list.js';
import {clamp} from './utils/utils.js';

declare global {
  interface DisplayMediaStreamOptions {
    // https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getDisplayMedia#systemaudio
    systemAudio?: 'exclude'|'include';
  }
}

const AUDIO_MIME_TYPE = 'audio/webm;codecs=opus';

const TIME_SLICE_MS = 100;

interface RecordingProgress {
  // Length in seconds.
  length: number;
  // All samples of the power. To conserve space while saving metadata with
  // JSON and since this is used for visualization only, the value will be
  // integer in range [0, 255], scaled from the original value of [0, 1].
  powers: number[];
  // This is used by the ongoing transcription view.
  textTokens: TextToken[];
}

/**
 * The source of the audio stream.
 */
export enum AudioSource {
  DISPLAY_MEDIA = 'DISPLAY_MEDIA',
  USER_MEDIA = 'USER_MEDIA',
}

function getStreamFromAudioSource(source: AudioSource): Promise<MediaStream> {
  switch (source) {
    case AudioSource.USER_MEDIA:
      return navigator.mediaDevices.getUserMedia({audio: true});
    case AudioSource.DISPLAY_MEDIA: {
      // TODO(shik): Handle the case that user cancelled the dialog, or stopped
      // sharing while recording.
      return navigator.mediaDevices.getDisplayMedia({
        video: false,
        audio: true,
        systemAudio: 'include',
      });
    }
    default:
      assertExhaustive(source);
  }
}

interface RecordingSessionConfig {
  source: AudioSource;
  platformHandler: PlatformHandler;
}

let audioCtxGlobal: AudioContext|null = null;

async function getAudioContext(): Promise<AudioContext> {
  if (audioCtxGlobal === null) {
    audioCtxGlobal = new AudioContext({sampleRate: SAMPLE_RATE});
    await audioCtxGlobal.audioWorklet.addModule('/static/audio_worklet.js');
  }
  return audioCtxGlobal;
}

/**
 * A recording session to retrieve audio input and produce an audio blob output.
 */
export class RecordingSession {
  private readonly dataChunks: Blob[] = [];

  private readonly sodaEventTransformer = new SodaEventTransformer();

  private readonly sodaSessionUnsubscribe: Unsubscribe;

  private readonly powers = signal<number[]>([]);

  private readonly textTokens = signal<TextToken[]>([]);

  readonly progress = computed<RecordingProgress>(() => {
    const powers = this.powers.value;
    const length = (powers.length * SAMPLES_PER_SLICE) / SAMPLE_RATE;
    return {
      length,
      powers,
      textTokens: this.textTokens.value,
    };
  });

  constructor(
    private readonly stream: MediaStream,
    private readonly mediaRecorder: MediaRecorder,
    private readonly audioProcessor: AudioWorkletNode,
    private readonly sodaSession: SodaSession,
  ) {
    this.mediaRecorder.addEventListener('dataavailable', (e) => {
      this.onDataAvailable(e);
    });
    this.mediaRecorder.addEventListener('error', (e) => {
      this.onError(e);
    });
    this.mediaRecorder.start(TIME_SLICE_MS);

    this.audioProcessor.port.addEventListener(
      'message',
      (ev: MessageEvent<Float32Array>) => {
        const samples = ev.data;
        // Calculates the power of the slice. The value range is [0, 1].
        const power = Math.sqrt(
          samples.map((v) => v * v).reduce((x, y) => x + y, 0) / samples.length,
        );
        const scaledPower = clamp(
          Math.floor(power * POWER_SCALE_FACTOR),
          0,
          POWER_SCALE_FACTOR - 1,
        );
        this.powers.mutate((d) => {
          d.push(scaledPower);
        });
        this.sodaSession.addAudio(samples);
      },
    );
    this.audioProcessor.port.start();
    this.sodaSessionUnsubscribe = this.sodaSession.subscribeEvent((ev) => {
      this.sodaEventTransformer.addEvent(ev);
      this.textTokens.value = this.sodaEventTransformer.getTokens();
    });
  }

  onDataAvailable(event: BlobEvent): void {
    // TODO(shik): Save the data to file system while recording.
    this.dataChunks.push(event.data);
  }

  onError(event: Event): void {
    // TODO(shik): Proper error handling.
    console.error(event);
  }

  async start(): Promise<void> {
    await this.sodaSession.start();
  }

  async finish(): Promise<Blob> {
    const stopped = new Promise((resolve) => {
      this.mediaRecorder.addEventListener('stop', resolve, {once: true});
    });
    this.mediaRecorder.stop();
    this.audioProcessor.port.close();
    await this.sodaSession.stop();
    this.sodaSessionUnsubscribe();
    await stopped;

    for (const track of this.stream.getTracks()) {
      track.stop();
    }

    return new Blob(this.dataChunks, {type: AUDIO_MIME_TYPE});
  }

  static async create(
    config: RecordingSessionConfig,
  ): Promise<RecordingSession> {
    const sodaSession = await config.platformHandler.newSodaSession();

    const stream = await getStreamFromAudioSource(config.source);
    const mediaRecorder = new MediaRecorder(stream, {
      mimeType: AUDIO_MIME_TYPE,
    });
    const audioCtx = await getAudioContext();
    const source = audioCtx.createMediaStreamSource(stream);
    const processor = new AudioWorkletNode(audioCtx, 'audio-processor');
    source.connect(processor);

    return new RecordingSession(stream, mediaRecorder, processor, sodaSession);
  }
}
