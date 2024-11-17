// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  ReactiveController,
  ReactiveControllerHost,
} from 'chrome://resources/mwc/lit/index.js';

import {useRecordingDataManager} from './lit/context.js';
import {ScopedAsyncEffect, ScopedEffect} from './reactive/lit.js';
import {computed, ReadonlySignal, Signal, signal} from './reactive/signal.js';
import {AnimationFrameController} from './utils/animation_frame_controller.js';
import {assertInstanceof} from './utils/assert.js';

/**
 * An Audio element with reactive controls.
 *
 * Note that this class is intended to be only used when there's a need of
 * passing playback progress between two UI instances. Normal user should use
 * AudioPlayerController instead.
 */
export interface ReactiveAudio {
  readonly recordingId: string|null;

  revoke(): void;
}

class ReactiveAudioImpl implements ReactiveAudio {
  private latestAudioSrc: string|null = null;

  private readonly currentTimeImpl = signal(0);

  private readonly playingImpl = signal(false);

  private readonly mutedImpl = signal(false);

  private readonly volumeImpl = signal(1);

  private readonly audio = new Audio();

  private readonly playbackSpeedImpl = signal(1);

  private readonly animationFrameController: AnimationFrameController;

  recordingId: string|null = null;

  readonly currentTime: Signal<number> = computed({
    get: () => {
      return this.currentTimeImpl.value;
    },
    set:
      (currentTime: number) => {
        this.audio.currentTime = currentTime;
      },
  });

  readonly playbackSpeed: Signal<number> = computed({
    get: () => {
      return this.playbackSpeedImpl.value;
    },
    set:
      (speed: number) => {
        // The speed can be changed while playback is paused, and the ratechange
        // event won't be immediately fired in this case. Since we want the
        // speed change to be immediately effective on UI and don't expect it to
        // fail for the list of speeds on the UI, we optimistic update the
        // `playbackSpeedImpl` here and log an error on the ratechange callback
        // if the actual speed is different from the set value.
        this.playbackSpeedImpl.value = speed;
        this.audio.playbackRate = speed;
      },
  });

  readonly muted: Signal<boolean> = computed({
    get: () => {
      return this.mutedImpl.value;
    },
    set:
      (muted: boolean) => {
        this.audio.muted = muted;
      },
  });

  readonly volume: Signal<number> = computed({
    get: () => {
      return this.volumeImpl.value;
    },
    set:
      (volume: number) => {
        this.audio.volume = volume;
      },
  });

  get playing(): ReadonlySignal<boolean> {
    return this.playingImpl;
  }

  constructor() {
    this.audio.addEventListener('ratechange', () => {
      if (this.audio.playbackRate !== this.playbackSpeedImpl.value) {
        // TODO(pihsun): Integrate with error reporting.
        // TODO(pihsun): Check if this will be fired on pause.
        console.warn(
          'Audio playback speed mismatch',
          this.audio.playbackRate,
          this.playbackSpeedImpl.value,
        );
      }
    });

    this.audio.addEventListener('volumechange', () => {
      this.volumeImpl.value = this.audio.volume;
      this.mutedImpl.value = this.audio.muted;
    });

    this.audio.addEventListener('timeupdate', () => {
      this.currentTimeImpl.value = this.audio.currentTime;
    });

    // While audio is playing, we also use AnimationFrameController in addition
    // of the timeupdate event, since timeupdate fires infrequently and doesn't
    // look smooth while playing.
    this.animationFrameController = new AnimationFrameController(() => {
      this.currentTimeImpl.value = this.audio.currentTime;
    });

    // Only run the animationFrameController when the audio is playing, to save
    // CPU cycle.
    this.audio.addEventListener('play', () => {
      this.playingImpl.value = true;
      this.animationFrameController.start();
    });
    this.audio.addEventListener('pause', () => {
      this.playingImpl.value = false;
      this.animationFrameController.stop();
    });
  }

  private revokeAudio(): void {
    if (this.latestAudioSrc !== null) {
      URL.revokeObjectURL(this.latestAudioSrc);
      this.latestAudioSrc = null;
    }
  }

  revoke(): void {
    this.audio.pause();
    this.revokeAudio();
    this.audio.src = '';
    this.animationFrameController.stop();
    this.recordingId = null;
  }

  play(): Promise<void> {
    return this.audio.play();
  }

  togglePlaying(): void {
    if (this.audio.paused) {
      // TODO(pihsun): This is async, should we await it?
      void this.audio.play();
    } else {
      this.audio.pause();
    }
  }

  loadAudioSrc(recordingId: string, src: string): void {
    this.recordingId = recordingId;
    this.revokeAudio();
    this.latestAudioSrc = src;
    this.audio.src = this.latestAudioSrc;
    this.audio.load();
  }
}

export class AudioPlayerController implements ReactiveController {
  private readonly recordingDataManager = useRecordingDataManager();

  // This is marked as protected to suppress the unused member error.
  protected readonly loadAudioData: ScopedAsyncEffect;

  protected readonly setMediaSessionTitle: ScopedEffect;

  private audio = new ReactiveAudioImpl();

  constructor(
    host: ReactiveControllerHost,
    private readonly recordingId: ReadonlySignal<string|null>,
    autoPlay = false,
  ) {
    host.addController(this);

    this.loadAudioData = new ScopedAsyncEffect(host, async (signal) => {
      const id = this.recordingId.value;
      if (id === null) {
        this.audio.revoke();
        return;
      }
      const data = await this.recordingDataManager.getAudioFile(id);
      signal.throwIfAborted();
      if (id === this.audio.recordingId) {
        // The inner audio is from the same recording, likely set by
        // setInnerAudio. Don't load the audio src and use the inner audio
        // state in this case.
        return;
      }
      this.audio.loadAudioSrc(id, URL.createObjectURL(data));
      if (autoPlay) {
        await this.audio.play();
      }
    });

    this.setMediaSessionTitle = new ScopedEffect(host, () => {
      const id = this.recordingId.value;
      if (id === null) {
        return;
      }
      const metadata = this.recordingDataManager.getMetadata(id).value;
      if (metadata === null) {
        return;
      }
      navigator.mediaSession.metadata = new MediaMetadata({
        title: metadata.title,
      });
    });
  }

  hostDisconnected(): void {
    this.audio.revoke();
  }

  get currentTime(): Signal<number> {
    return this.audio.currentTime;
  }

  get playbackSpeed(): Signal<number> {
    return this.audio.playbackSpeed;
  }

  get muted(): Signal<boolean> {
    return this.audio.muted;
  }

  get volume(): Signal<number> {
    return this.audio.volume;
  }

  get playing(): ReadonlySignal<boolean> {
    return this.audio.playing;
  }

  togglePlaying(): void {
    this.audio.togglePlaying();
  }

  // Note that caller needs to make sure that the `.revoke()` on the returning
  // ReactiveAudio is eventually called, otherwise the AnimationFrameController
  // would leak.
  takeInnerAudio(): ReactiveAudio {
    const audio = this.audio;
    // TODO(pihsun): Creating a new inner object to prevent revoking on
    // hostDisconnected is a bit wasteful.
    this.audio = new ReactiveAudioImpl();
    return audio;
  }

  setInnerAudio(audio: ReactiveAudio): void {
    const impl = assertInstanceof(audio, ReactiveAudioImpl);
    this.audio.revoke();
    this.audio = impl;
  }
}
