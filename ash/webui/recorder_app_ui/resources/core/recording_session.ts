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
import {SodaEventTransformer, Transcription} from './soda/soda.js';
import {SodaSession} from './soda/types.js';
import {
  assert,
  assertExhaustive,
  assertExists,
  assertNotReached,
} from './utils/assert.js';
import {AsyncJobInfo, AsyncJobQueue} from './utils/async_job_queue.js';
import {InteriorMutableArray} from './utils/interior_mutable_array.js';
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
  powers: InteriorMutableArray<number>;
  // Transcription of the ongoing recording. null if transcription is never
  // enabled throughout the recording.
  transcription: Transcription|null;
}

function getMicrophoneStream(
  micId: string,
  echoCancellation: boolean,
): Promise<MediaStream> {
  return navigator.mediaDevices.getUserMedia({
    audio: {
      deviceId: {exact: micId},
      echoCancellation: {exact: echoCancellation},
    },
  });
}

interface RecordingSessionConfig {
  includeSystemAudio: boolean;
  micId: string;
  platformHandler: PlatformHandler;
  speakerLabelEnabled: boolean;
  canCaptureSystemAudioWithLoopback: boolean;
}

let audioCtxGlobal: AudioContext|null = null;

async function getAudioContext(): Promise<AudioContext> {
  if (audioCtxGlobal === null) {
    // Set null output device when recording.
    audioCtxGlobal = new AudioContext({
      sampleRate: SAMPLE_RATE,
      sinkId: {type: 'none'},
    });
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

  private readonly sodaEventTransformer: SodaEventTransformer;

  private currentSodaSession: SodaSessionInfo|null = null;

  private readonly sodaEnableQueue = new AsyncJobQueue('keepLatest');

  private readonly powers = signal(new InteriorMutableArray<number>([]));

  private readonly transcription = signal<Transcription|null>(null);

  private processedSamples = 0;

  private readonly mediaRecorder: MediaRecorder;

  private readonly audioProcessor: AudioWorkletNode;

  private readonly combinedInputNode: MediaStreamAudioDestinationNode;

  private micAudioSourceNode: MediaStreamAudioSourceNode|null = null;

  private systemAudioSourceNode: MediaStreamAudioSourceNode|null = null;

  private micMuted = false;

  private everPausedInternal = false;

  private everMutedInternal = false;

  readonly progress = computed<RecordingProgress>(() => {
    const powers = this.powers.value;
    const length = (powers.length * SAMPLES_PER_SLICE) / SAMPLE_RATE;
    return {
      length,
      powers,
      transcription: this.transcription.value,
    };
  });

  private constructor(
    private readonly audioCtx: AudioContext,
    private readonly config: RecordingSessionConfig,
  ) {
    this.sodaEventTransformer = new SodaEventTransformer(
      config.speakerLabelEnabled,
    );
    this.combinedInputNode = audioCtx.createMediaStreamDestination();
    this.audioProcessor = new AudioWorkletNode(audioCtx, 'audio-processor');
    this.mediaRecorder = new MediaRecorder(this.combinedInputNode.stream, {
      mimeType: AUDIO_MIME_TYPE,
    });

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
        this.powers.value = this.powers.value.push(scaledPower);
        this.currentSodaSession?.session.addAudio(samples);
        this.processedSamples += samples.length;
      },
    );
  }

  /**
   * Sets the mute state of the mic stream.
   *
   * Note that this doesn't change the state of the system audio stream, as the
   * mute button is intended to only mute the mic stream.
   */
  setMicMuted(muted: boolean): void {
    this.micMuted = muted;
    if (muted) {
      this.everMutedInternal = true;
    }
    if (this.micAudioSourceNode !== null) {
      for (const track of this.micAudioSourceNode.mediaStream.getTracks()) {
        track.enabled = !muted;
      }
    }
  }

  get everPaused(): boolean {
    return this.everPausedInternal;
  }

  get everMuted(): boolean {
    return this.everMutedInternal;
  }

  private connectSourceNode(node: MediaStreamAudioSourceNode) {
    node.connect(this.combinedInputNode);
    node.connect(this.audioProcessor);
  }

  private async initMicAudioSourceNode() {
    if (this.micAudioSourceNode !== null) {
      return;
    }

    // Turn on AEC when capturing system audio via getDisplayMedia.
    const micStream = await getMicrophoneStream(
      this.config.micId,
      this.config.canCaptureSystemAudioWithLoopback,
    );
    this.micAudioSourceNode = this.audioCtx.createMediaStreamSource(micStream);
    this.connectSourceNode(this.micAudioSourceNode);

    // Set the mic muted setting again onto the new mic stream.
    this.setMicMuted(this.micMuted);
  }

  private async initSystemAudioSourceNode() {
    if (this.systemAudioSourceNode !== null) {
      return;
    }

    if (!this.config.includeSystemAudio ||
        !this.config.canCaptureSystemAudioWithLoopback) {
      return;
    }

    const systemAudioStream =
      await this.config.platformHandler.getSystemAudioMediaStream();
    this.systemAudioSourceNode =
      this.audioCtx.createMediaStreamSource(systemAudioStream);
    this.connectSourceNode(this.systemAudioSourceNode);
  }

  private closeAudioSourceNode(node: MediaStreamAudioSourceNode) {
    for (const track of node.mediaStream.getTracks()) {
      track.stop();
    }
    node.disconnect();
  }

  private closeMicAudioSourceNode() {
    if (this.micAudioSourceNode !== null) {
      this.closeAudioSourceNode(this.micAudioSourceNode);
      this.micAudioSourceNode = null;
    }
  }

  private closeSystemAudioSourceNode() {
    if (this.systemAudioSourceNode !== null) {
      this.closeAudioSourceNode(this.systemAudioSourceNode);
      this.systemAudioSourceNode = null;
    }
  }

  async setPaused(paused: boolean): Promise<void> {
    if (paused) {
      await this.audioCtx.suspend();
      // We still need to explicitly pause the media recorder, otherwise the
      // exported webm will have wrong timestamps.
      this.mediaRecorder.pause();
      // Close the mic when paused, so the "mic in use" indicator would go away.
      this.closeMicAudioSourceNode();
      this.everPausedInternal = true;
    } else {
      await this.initMicAudioSourceNode();
      this.mediaRecorder.resume();
      await this.audioCtx.resume();
    }
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
    const {platformHandler} = this.config;
    const sodaState = platformHandler.sodaState;
    assert(
      sodaState.value.kind !== 'unavailable',
      `Trying to install SODA when it's unavailable`,
    );
    if (sodaState.value.kind === 'installed') {
      return;
    }
    platformHandler.installSoda();
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
      if (this.transcription.value === null) {
        this.transcription.value = new Transcription([]);
      }
      await this.ensureSodaInstalled();
      // Abort current running job if there's a new enable/disable request.
      if (this.sodaEnableQueue.hasPendingJob()) {
        return;
      }

      const session = await this.config.platformHandler.newSodaSession();
      const unsubscribe = session.subscribeEvent((ev) => {
        this.sodaEventTransformer.addEvent(
          ev,
          assertExists(this.currentSodaSession).startOffsetMs,
        );
        this.transcription.value = this.sodaEventTransformer.getTranscription();
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
      // TODO: b/369277555 - Investigate why SODA does not convert all results
      // to final.
      this.transcription.value = this.sodaEventTransformer.getTranscription(
        /* shouldFinalizeTranscription= */ true,
      );
      this.currentSodaSession = null;
    });
  }

  /**
   * Starts the recording session.
   *
   * Note that each recording session is intended to only be started once.
   */
  async start(transcriptionEnabled: boolean): Promise<void> {
    // Suspend the context while initializing the source nodes.
    await this.audioCtx.suspend();

    if (transcriptionEnabled) {
      // If the transcription is enabled from the beginning, await for the soda
      // session to start to avoid having start of audio not transcribed.
      // TODO(pihsun): Should this be happened asynchronously and have the
      // audio buffered?
      await this.startNewSodaSession().result;
    }

    await Promise.all([
      this.initMicAudioSourceNode(),
      this.initSystemAudioSourceNode(),
    ]);

    // Resume the context and start the recorder & audio processor after we've
    // initialized all sources.
    await this.audioCtx.resume();

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

    this.closeMicAudioSourceNode();
    this.closeSystemAudioSourceNode();

    return new Blob(this.dataChunks, {type: AUDIO_MIME_TYPE});
  }

  static async create(
    config: RecordingSessionConfig,
  ): Promise<RecordingSession> {
    const audioCtx = await getAudioContext();
    return new RecordingSession(audioCtx, config);
  }
}
