// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  POWER_SCALE_FACTOR,
  SAMPLE_RATE,
  SAMPLES_PER_SLICE,
} from './audio_constants.js';
import {PlatformHandler} from './platform_handler.js';
import {computed, effect, signal} from './reactive/signal.js';
import {SodaEventTransformer, TextToken} from './soda/soda.js';
import {SodaSession} from './soda/types.js';
import {
  assert,
  assertExhaustive,
  assertExists,
  assertNotReached,
} from './utils/assert.js';
import {AsyncJobInfo, AsyncJobQueue} from './utils/async_job_queue.js';
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
  // Transcription of the ongoing recording. null if transcription is never
  // enabled throughout the recording.
  textTokens: TextToken[]|null;
}

function getMicrophoneStream(micId: string): Promise<MediaStream> {
  return navigator.mediaDevices.getUserMedia({
    audio: {
      deviceId: {exact: micId},
    },
  });
}

interface RecordingSessionConfig {
  includeSystemAudio: boolean;
  micId: string;
  platformHandler: PlatformHandler;
}

let audioCtxGlobal: AudioContext|null = null;

async function getAudioContext(): Promise<AudioContext> {
  if (audioCtxGlobal === null) {
    audioCtxGlobal = new AudioContext({sampleRate: SAMPLE_RATE});
    await audioCtxGlobal.audioWorklet.addModule('./static/audio_worklet.js');
  }
  return audioCtxGlobal;
}

interface SodaSessionInfo {
  session: SodaSession;
  startOffsetMs: number;
  unsubscribe: Unsubscribe;
}

/**
 * A recording session to retrieve audio input and produce an audio blob output.
 */
export class RecordingSession {
  private readonly dataChunks: Blob[] = [];

  private readonly sodaEventTransformer = new SodaEventTransformer();

  private currentSodaSession: SodaSessionInfo|null = null;

  private readonly sodaEnableQueue = new AsyncJobQueue('keepLatest');

  private readonly powers = signal<number[]>([]);

  private readonly textTokens = signal<TextToken[]|null>(null);

  private processedSamples = 0;

  readonly progress = computed<RecordingProgress>(() => {
    const powers = this.powers.value;
    const length = (powers.length * SAMPLES_PER_SLICE) / SAMPLE_RATE;
    return {
      length,
      powers,
      textTokens: this.textTokens.value,
    };
  });

  private constructor(
    private readonly platformHandler: PlatformHandler,
    private readonly sourceStreams: MediaStream[],
    private readonly stream: MediaStream,
    private readonly mediaRecorder: MediaRecorder,
    private readonly audioProcessor: AudioWorkletNode,
  ) {
    this.mediaRecorder.addEventListener('dataavailable', (e) => {
      this.onDataAvailable(e);
    });
    this.mediaRecorder.addEventListener('error', (e) => {
      this.onError(e);
    });

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
        this.currentSodaSession?.session.addAudio(samples);
        this.processedSamples += samples.length;
      },
    );
  }

  private onDataAvailable(event: BlobEvent): void {
    // TODO(shik): Save the data to file system while recording.
    this.dataChunks.push(event.data);
  }

  private onError(event: Event): void {
    // TODO(shik): Proper error handling.
    console.error(event);
  }

  private async ensureSodaInstalled(): Promise<void> {
    const sodaState = this.platformHandler.sodaState;
    assert(
      sodaState.value.kind !== 'unavailable',
      `Trying to install SODA when it's unavailable`,
    );
    if (sodaState.value.kind === 'installed') {
      return;
    }
    this.platformHandler.installSoda();
    await new Promise<void>((resolve, reject) => {
      effect(({dispose}) => {
        switch (sodaState.value.kind) {
          case 'error':
            dispose();
            reject(new Error('Install SODA failed'));
            break;
          case 'installed':
            dispose();
            resolve();
            break;
          case 'notInstalled':
          case 'installing':
            break;
          case 'unavailable':
            return assertNotReached(
              `Trying to install SODA when it's unavailable`,
            );
          default:
            assertExhaustive(sodaState.value);
        }
      });
    });
  }

  startNewSodaSession(): AsyncJobInfo {
    return this.sodaEnableQueue.push(async () => {
      if (this.currentSodaSession !== null) {
        return;
      }
      if (this.textTokens.value === null) {
        this.textTokens.value = [];
      }
      await this.ensureSodaInstalled();
      // Abort current running job if there's a new enable/disable request.
      if (this.sodaEnableQueue.hasPendingJob()) {
        return;
      }

      const session = await this.platformHandler.newSodaSession();
      const unsubscribe = session.subscribeEvent((ev) => {
        this.sodaEventTransformer.addEvent(
          ev,
          assertExists(this.currentSodaSession).startOffsetMs,
        );
        this.textTokens.value = this.sodaEventTransformer.getTokens();
      });
      this.currentSodaSession = {
        session,
        unsubscribe,
        startOffsetMs: (this.processedSamples / SAMPLE_RATE) * 1000,
      };
      await session.start();
    });
  }

  stopSodaSession(): AsyncJobInfo {
    return this.sodaEnableQueue.push(async () => {
      if (this.currentSodaSession === null) {
        return;
      }
      await this.currentSodaSession.session.stop();
      this.currentSodaSession.unsubscribe();
      this.currentSodaSession = null;
    });
  }

  /**
   * Starts the recording session.
   *
   * Note that each recording session is intended to only be started once.
   * TODO(pihsun): Have function for pause/resume the recording.
   */
  async start(transcriptionEnabled: boolean): Promise<void> {
    if (transcriptionEnabled) {
      // If the transcription is enabled from the beginning, await for the soda
      // session to start to avoid having start of audio not transcribed.
      // TODO(pihsun): Should this be happened asynchronously and have the
      // audio buffered?
      await this.startNewSodaSession().result;
    }
    this.audioProcessor.port.start();
    this.mediaRecorder.start(TIME_SLICE_MS);
  }

  async finish(): Promise<Blob> {
    const stopped = new Promise((resolve) => {
      this.mediaRecorder.addEventListener('stop', resolve, {once: true});
    });
    this.mediaRecorder.stop();
    this.audioProcessor.port.close();
    await this.stopSodaSession().result;
    await stopped;

    for (const stream of [this.stream, ...this.sourceStreams]) {
      for (const track of stream.getTracks()) {
        track.stop();
      }
    }

    return new Blob(this.dataChunks, {type: AUDIO_MIME_TYPE});
  }

  static async create(
    config: RecordingSessionConfig,
  ): Promise<RecordingSession> {
    const requestingStreams = [getMicrophoneStream(config.micId)];
    if (config.includeSystemAudio) {
      requestingStreams.push(
        config.platformHandler.getSystemAudioMediaStream(),
      );
    }
    const streams = await Promise.all(requestingStreams);

    const audioCtx = await getAudioContext();
    const combinedInput = audioCtx.createMediaStreamDestination();
    const processor = new AudioWorkletNode(audioCtx, 'audio-processor');

    for (const stream of streams) {
      const source = audioCtx.createMediaStreamSource(stream);
      source.connect(combinedInput);
      source.connect(processor);
    }

    const combinedStream = combinedInput.stream;
    const mediaRecorder = new MediaRecorder(combinedStream, {
      mimeType: AUDIO_MIME_TYPE,
    });

    return new RecordingSession(
      config.platformHandler,
      streams,
      combinedStream,
      mediaRecorder,
      processor,
    );
  }
}
