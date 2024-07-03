// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is the low-level class that generates ChromeVox's
 * earcons. It's designed to be self-contained and not depend on the
 * rest of the code.
 */

import {EarconId} from '../common/earcon_id.js';

interface PlayProperties {
  pitch?: number;
  time?: number;
  gain?: number;
  loop?: boolean;
  pan?: number;
  reverb?: number;
}

interface GenerateSinusoidalProperties {gain: number;
            freq: number;
            endFreq?: number;
            time?: number;
            overtones?: number;
            overtoneFactor?: number;
            dur?: number;
            attack?: number;
            decay?: number;
            pan?: number;
            reverb?: number;
          }

/** EarconEngine generates ChromeVox's earcons using the web audio API. */
export class EarconEngine {

  // Public control parameters. All of these are meant to be adjustable.

  /** The output volume, as an amplification factor. */
  outputVolume = 1.0;

  /**
   * As notated below, all pitches are in the key of C. This can be set to
   * transpose the key from C to another pitch.
   */
  transposeToKey = Note.B_FLAT3;

  /** The click volume, as an amplification factor. */
  clickVolume = 0.4;

  /**
   * The volume of the static sound, as an amplification factor.
   */
  staticVolume = 0.2;

  /** The base delay for repeated sounds, in seconds. */
  baseDelay = 0.045;

  /** The base stereo panning, from -1 to 1. */
  basePan = CENTER_PAN;

  /** The base reverb level as an amplification factor. */
  baseReverb = 0.4;

  /** The choice of the reverb impulse response to use. */
  reverbSound = Reverb.SMALL_ROOM;

  /** The base pitch for the 'wrap' sound. */
  wrapPitch = Note.G_FLAT3;

  /** The base pitch for the 'alert' sound. */
  alertPitch = Note.G_FLAT3;

  /** The default pitch. */
  defaultPitch = Note.G3;

  /** The choice of base sound for most controls. */
  controlSound = WavSoundFile.CONTROL;

  /**
   * The delay between sounds in the on/off sweep effect,
   * in seconds.
   */
  sweepDelay = 0.045;

  /**
   * The delay between echos in the on/off sweep, in seconds.
   */
  sweepEchoDelay = 0.15;

  /** The number of echos in the on/off sweep. */
  sweepEchoCount = 3;

  /** The pitch offset of the on/off sweep. */
  sweepPitch = Note.C3;

  /**
   * The final gain of the progress sound, as an
   * amplification factor.
   */
  progressFinalGain = 0.05;

  /** The multiplicative decay rate of the progress ticks. */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  progressGain_Decay = 0.7;

  // Private variables.

  /** The audio context. */
  private context_ = new AudioContext();

  /** The reverb node, lazily initialized. */
  private reverbConvolver_: ConvolverNode | null = null;

  /**
   * @private A map between the name of an audio data file and its loaded AudioBuffer.
   */
  private buffers_: { [key: string]: AudioBuffer } = {};

  private loops_: { [key: string]: AudioBufferSourceNode } = {};

  /**
   * The source audio nodes for queued tick / tocks for progress.
   * Kept around so they can be canceled.
   */
  private progressSources_: Array<[number,AudioNode]> = [];

  /** The current gain for progress sounds. */
  private progressGain_ = 1.0;

  /** The current time for progress sounds. */
  private progressTime_?: number = this.context_.currentTime;

  /** The setInterval ID for progress sounds. */
  private progressIntervalID_: number | null = null;

  /**
   * Maps a earcon name to the last source input audio for that
   * earcon.
   */
  private lastEarconSources_: Partial<Record<EarconId, AudioNode>> = {};

  private currentTrackedEarcon_?: EarconId;

  constructor() {

    // Initialization: load the base sound data files asynchronously.
    Object.values(WavSoundFile)
      .concat(Object.values(Reverb))
      .forEach(sound => this.loadSound(sound, `${BASE_URL}${sound}.wav`));
    Object.values(OggSoundFile)
      .forEach(sound => this.loadSound(sound, `${BASE_URL}${sound}.ogg`));
  }

  /**
   * A high-level way to ask the engine to play a specific earcon.
   * @param earcon The earcon to play.
   */
  playEarcon(earcon: EarconId): void {
    // These earcons are not tracked by the engine via their audio sources.
    switch (earcon) {
      case EarconId.CHROMEVOX_LOADED:
        this.onChromeVoxLoaded();
        return;
      case EarconId.CHROMEVOX_LOADING:
        this.onChromeVoxLoading();
        return;
      case EarconId.PAGE_FINISH_LOADING:
        this.cancelProgress();
        return;
      case EarconId.PAGE_START_LOADING:
        this.startProgress();
        return;
      case EarconId.POP_UP_BUTTON:
        this.onPopUpButton();
        return;

      // These had earcons in previous versions of ChromeVox but
      // they're currently unused / unassigned.
      case EarconId.LIST_ITEM:
      case EarconId.LONG_DESC:
      case EarconId.MATH:
      case EarconId.OBJECT_CLOSE:
      case EarconId.OBJECT_ENTER:
      case EarconId.OBJECT_EXIT:
      case EarconId.OBJECT_OPEN:
      case EarconId.OBJECT_SELECT:
      case EarconId.RECOVER_FOCUS:
        return;
    }

    // These earcons are tracked by the engine via their audio sources.
    if (this.lastEarconSources_[earcon] !== undefined) {
      // Playback of |earcon| is in progress.
      return;
    }

    this.currentTrackedEarcon_ = earcon;
    switch (earcon) {
      case EarconId.ALERT_MODAL:
      case EarconId.ALERT_NONMODAL:
        this.onAlert();
        break;
      case EarconId.BUTTON:
        this.onButton();
        break;
      case EarconId.CHECK_OFF:
        this.onCheckOff();
        break;
      case EarconId.CHECK_ON:
        this.onCheckOn();
        break;
      case EarconId.EDITABLE_TEXT:
        this.onTextField();
        break;
      case EarconId.INVALID_KEYPRESS:
        this.onInvalidKeypress();
        break;
      case EarconId.LINK:
        this.onLink();
        break;
      case EarconId.LISTBOX:
        this.onSelect();
        break;
      case EarconId.SELECTION:
        this.onSelection();
        break;
      case EarconId.SELECTION_REVERSE:
        this.onSelectionReverse();
        break;
      case EarconId.SKIP:
        this.onSkim();
        break;
      case EarconId.SLIDER:
        this.onSlider();
        break;
      case EarconId.SMART_STICKY_MODE_OFF:
        this.onSmartStickyModeOff();
        break;
      case EarconId.SMART_STICKY_MODE_ON:
        this.onSmartStickyModeOn();
        break;
      case EarconId.NO_POINTER_ANCHOR:
        this.onNoPointerAnchor();
        break;
      case EarconId.WRAP:
      case EarconId.WRAP_EDGE:
        this.onWrap();
        break;
    }
    this.currentTrackedEarcon_ = undefined;

    // Clear source once it finishes playing.
    const source = this.lastEarconSources_[earcon];
    if (source !== undefined &&
        (source as any) instanceof AudioScheduledSourceNode) {
      (source as AudioScheduledSourceNode).onended = () => {
        delete this.lastEarconSources_[earcon];
      };
    }
  }

  /**
   * Fetches a sound asynchronously and loads its data into an AudioBuffer.
   *
   * @param name The name of the sound to load.
   * @param url The url where the sound should be fetched from.
   */
  async loadSound(name: string, url: string): Promise<void> {
    const response = await fetch(url);
    if (response.ok) {
      const arrayBuffer = await response.arrayBuffer();
      const decodedAudio = await this.context_.decodeAudioData(arrayBuffer);
      this.buffers_[name] = decodedAudio;
    }
  }

  /**
   * Return an AudioNode containing the final processing that all
   * sounds go through: output volume / gain, panning, and reverb.
   * The chain is hooked up to the destination automatically, so you
   * just need to connect your source to the return value from this
   * method.
   *
   * @param properties
   *     An object where you can override the default
   *     gain, pan, and reverb, otherwise these are taken from
   *     outputVolume, basePan, and baseReverb.
   * @return The filters to be applied to all sounds, connected
   *     to the destination node.
   */
  createCommonFilters(properties: { gain?: number, pan?: number, reverb?: number }): AudioNode {
    let gain = this.outputVolume;
    if (properties.gain) {
      gain *= properties.gain;
    }
    const gainNode = this.context_.createGain();
    gainNode.gain.value = gain;
    const first = gainNode;
    let last: AudioNode = gainNode;

    const pan = properties.pan ?? this.basePan;
    if (pan !== 0) {
      const panNode = this.context_.createPanner();
      panNode.setPosition(pan, 0, 0);
      panNode.setOrientation(0, 0, 1);
      last.connect(panNode);
      last = panNode;
    }

    const reverb = properties.reverb ?? this.baseReverb;
    if (reverb) {
      if (!this.reverbConvolver_) {
        this.reverbConvolver_ = this.context_.createConvolver();
        this.reverbConvolver_.buffer = this.buffers_[this.reverbSound];
        this.reverbConvolver_.connect(this.context_.destination);
      }

      // Dry
      last.connect(this.context_.destination);

      // Wet
      const reverbGainNode = this.context_.createGain();
      reverbGainNode.gain.value = reverb;
      last.connect(reverbGainNode);
      reverbGainNode.connect(this.reverbConvolver_);
    } else {
      last.connect(this.context_.destination);
    }

    return first;
  }

  /**
   * High-level interface to play a sound from a buffer source by name,
   * with some simple adjustments like pitch change (in half-steps),
   * a start time (relative to the current time, in seconds),
   * gain, panning, and reverb.
   *
   * The only required parameter is the name of the sound. The time, pitch,
   * gain, panning, and reverb are all optional and are passed in an
   * object of optional properties.
   *
   * @param sound The name of the sound to play. It must already
   *     be loaded in a buffer.
   * @param properties
   *     An object where you can override the default pitch, gain, pan,
   *     and reverb.
   * @return The source node, so you can stop it
   *     or set event handlers on it.
   */
  play(sound: string, properties: PlayProperties  = {}): AudioBufferSourceNode {
    const source = this.context_.createBufferSource();
    source.buffer = this.buffers_[sound];
    if (properties.loop) {
      this.loops_[sound] = source;
    }

    const pitch = properties.pitch ?? this.defaultPitch;
    // Changes the playback rate of the sample – which also changes the pitch.
    source.playbackRate.value = this.multiplierFor_(pitch);
    source.loop = properties.loop ?? false;

    const destination = this.createCommonFilters(properties);
    source.connect(destination);
    if (this.currentTrackedEarcon_) {
      this.lastEarconSources_[this.currentTrackedEarcon_] = source;
    }
    if (properties.time) {
      source.start(this.context_.currentTime + properties.time);
    } else {
      source.start(this.context_.currentTime);
    }

    return source;
  }

  /**
   * Stops the loop of the specified sound file, if one exists.
   * @param sound The name of the sound file.
   */
  stopLoop(sound: string): void {
    if (!this.loops_[sound]) {
      return;
    }

    this.loops_[sound].stop();
    delete this.loops_[sound];
  }

  /** Play the static sound. */
  onStatic(): void {
    this.play(WavSoundFile.STATIC, { gain: this.staticVolume });
  }

  /** Play the link sound. */
  onLink(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume });
    this.play(this.controlSound, { pitch: Note.G4 });
  }

  /** Play the button sound. */
  onButton(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume });
    this.play(this.controlSound);
  }

  /** Play the text field sound. */
  onTextField(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume });
    this.play(
      WavSoundFile.STATIC,
      { time: this.baseDelay * 1.5, gain: this.clickVolume * 0.5 });
    this.play(this.controlSound, { pitch: Note.B3 });
    this.play(
      this.controlSound,
      { pitch: Note.B3, time: this.baseDelay * 1.5, gain: 0.5 });
  }

  /** Play the pop up button sound. */
  onPopUpButton(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume });

    this.play(this.controlSound);
    this.play(
      this.controlSound,
      { time: this.baseDelay * 3, gain: 0.2, pitch: Note.G4 });
    this.play(
      this.controlSound,
      { time: this.baseDelay * 4.5, gain: 0.2, pitch: Note.G4 });
  }

  /** Play the check on sound. */
  onCheckOn(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume });
    this.play(this.controlSound, { pitch: Note.D3 });
    this.play(this.controlSound, { pitch: Note.D4, time: this.baseDelay * 2 });
  }

  /** Play the check off sound. */
  onCheckOff(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume });
    this.play(this.controlSound, { pitch: Note.D4 });
    this.play(this.controlSound, { pitch: Note.D3, time: this.baseDelay * 2 });
  }

  /** Play the smart sticky mode on sound. */
  onSmartStickyModeOn(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume * 0.5 });
    this.play(this.controlSound, { pitch: Note.D4 });
  }

  /** Play the smart sticky mode off sound. */
  onSmartStickyModeOff(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume * 0.5 });
    this.play(this.controlSound, { pitch: Note.D3 });
  }

  /** Play the select control sound. */
  onSelect(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume });
    this.play(this.controlSound);
    this.play(this.controlSound, { time: this.baseDelay });
    this.play(this.controlSound, { time: this.baseDelay * 2 });
  }

  /** Play the slider sound. */
  onSlider(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume });
    this.play(this.controlSound);
    this.play(
      this.controlSound, { time: this.baseDelay, gain: 0.5, pitch: Note.A3 });
    this.play(
      this.controlSound,
      { time: this.baseDelay * 2, gain: 0.25, pitch: Note.B3 });
    this.play(
      this.controlSound,
      { time: this.baseDelay * 3, gain: 0.125, pitch: Note.D_FLAT4 });
    this.play(
      this.controlSound,
      { time: this.baseDelay * 4, gain: 0.0625, pitch: Note.E_FLAT4 });
  }

  /** Play the skim sound. */
  onSkim(): void {
    this.play(WavSoundFile.SKIM);
  }

  /** Play the selection sound. */
  onSelection(): void {
    this.play(OggSoundFile.SELECTION);
  }

  /** Play the selection reverse sound. */
  onSelectionReverse(): void {
    this.play(OggSoundFile.SELECTION_REVERSE);
  }

  /** Play the invalid keypress sound. */
  onInvalidKeypress(): void {
    this.play(OggSoundFile.INVALID_KEYPRESS);
  }

  onNoPointerAnchor(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume * 0.2 });
    const freq1 = this.frequencyFor_(Note.A_FLAT4);
    this.generateSinusoidal({
      attack: 0.00001,
      decay: 0.01,
      dur: 0.1,
      gain: 0.008,
      freq: freq1,
      overtones: 1,
      overtoneFactor: 0.1,
    });
  }

  /**
   * Generate a synthesized musical note based on a sum of sinusoidals shaped
   * by an envelope, controlled by a number of properties.
   *
   * The sound has a frequency of |freq|, or if |endFreq| is specified, does
   * an exponential ramp from |freq| to |endFreq|.
   *
   * If |overtones| is greater than 1, the sound will be mixed with additional
   * sinusoidals at multiples of |freq|, each one scaled by |overtoneFactor|.
   * This creates a rounder tone than a pure sine wave.
   *
   * The envelope is shaped by the duration |dur|, the attack time |attack|,
   * and the decay time |decay|, in seconds.
   *
   * As with other functions, |pan| and |reverb| can be used to override
   * basePan and baseReverb.
   *
   * @param properties
   *     An object containing the properties that can be used to
   *     control the sound, as described above.
   */
  generateSinusoidal(properties:GenerateSinusoidalProperties): void {
    const envelopeNode = this.context_.createGain();
    envelopeNode.connect(this.context_.destination);

    const time = properties.time ?? 0;

    // Generate an oscillator for the frequency corresponding to the specified
    // frequency, and then additional overtones at multiples of that frequency
    // scaled by the overtoneFactor. Cue the oscillator to start and stop
    // based on the start time and specified duration.
    //
    // If an end frequency is specified, do an exponential ramp to that end
    // frequency.
    let gain = properties.gain;
    // TODO(b/314203187): Determine if not null assertion is acceptable.
    for (let i = 0; i < properties.overtones!; i++) {
      const osc = this.context_.createOscillator();
      if (this.currentTrackedEarcon_) {
        this.lastEarconSources_[this.currentTrackedEarcon_] = osc;
      }
      osc.frequency.value = properties.freq * (i + 1);

      if (properties.endFreq) {
        osc.frequency.setValueAtTime(
          properties.freq * (i + 1), this.context_.currentTime + time);
        osc.frequency.exponentialRampToValueAtTime(
          properties.endFreq * (i + 1),
          this.context_.currentTime + properties.dur!);
      }

      osc.start(this.context_.currentTime + time);
      osc.stop(this.context_.currentTime + time + properties.dur!);

      const gainNode = this.context_.createGain();
      gainNode.gain.value = gain;
      osc.connect(gainNode);
      gainNode.connect(envelopeNode);

      gain *= properties.overtoneFactor!;
    }

    // Shape the overall sound by an envelope based on the attack and
    // decay times.
     // TODO(b/314203187): Determine if not null assertion is acceptable.
    envelopeNode.gain.setValueAtTime(0, this.context_.currentTime + time);
    envelopeNode.gain.linearRampToValueAtTime(
      1, this.context_.currentTime + time + properties.attack!);
    envelopeNode.gain.setValueAtTime(
      1,
      this.context_.currentTime + time + properties.dur! - properties.decay!);
    envelopeNode.gain.linearRampToValueAtTime(
      0, this.context_.currentTime + time + properties.dur!);

    // Route everything through the common filters like reverb at the end.
    const destination = this.createCommonFilters({});
    envelopeNode.connect(destination);
  }

  /** Play an alert sound. */
  onAlert(): void {
    const freq1 = this.frequencyFor_(this.alertPitch - 2);
    const freq2 = this.frequencyFor_(this.alertPitch - 3);
    this.generateSinusoidal({
      attack: 0.02,
      decay: 0.07,
      dur: 0.15,
      gain: 0.3,
      freq: freq1,
      overtones: 3,
      overtoneFactor: 0.1,
    });
    this.generateSinusoidal({
      attack: 0.02,
      decay: 0.07,
      dur: 0.15,
      gain: 0.3,
      freq: freq2,
      overtones: 3,
      overtoneFactor: 0.1,
    });

    this.currentTrackedEarcon_ = undefined;
  }

  /** Play a wrap sound. */
  onWrap(): void {
    this.play(WavSoundFile.STATIC, { gain: this.clickVolume * 0.3 });
    const freq1 = this.frequencyFor_(this.wrapPitch - 8);
    const freq2 = this.frequencyFor_(this.wrapPitch + 8);
    this.generateSinusoidal({
      attack: 0.01,
      decay: 0.1,
      dur: 0.15,
      gain: 0.3,
      freq: freq1,
      endFreq: freq2,
      overtones: 1,
      overtoneFactor: 0.1,
    });
  }

  /**
   * Queue up a few tick/tock sounds for a progress bar. This is called
   * repeatedly by setInterval to keep the sounds going continuously.
   */
  private generateProgressTickTocks_(): void {
     // TODO(b/314203187): Determine if not null assertion is acceptable.
     this.progressTime_ = this.progressTime_!;
    while (this.progressTime_ < this.context_.currentTime + 3.0) {
      let t = this.progressTime_ - this.context_.currentTime;
      this.progressSources_.push([
        this.progressTime_,
        this.play(
          WavSoundFile.STATIC, { gain: 0.5 * this.progressGain_, time: t }),
      ]);
      this.progressSources_.push([
        this.progressTime_,
        this.play(
          this.controlSound,
          { pitch: Note.E_FLAT5, time: t, gain: this.progressGain_ }),
      ]);

      if (this.progressGain_ > this.progressFinalGain) {
        this.progressGain_ *= this.progressGain_Decay;
      }
      t += 0.5;

      this.progressSources_.push([
        this.progressTime_,
        this.play(
          WavSoundFile.STATIC, { gain: 0.5 * this.progressGain_, time: t }),
      ]);
      this.progressSources_.push([
        this.progressTime_,
        this.play(
          this.controlSound,
          { pitch: Note.E_FLAT4, time: t, gain: this.progressGain_ }),
      ]);

      if (this.progressGain_ > this.progressFinalGain) {
        this.progressGain_ *= this.progressGain_Decay;
      }

      this.progressTime_! += 1.0;
    }

    let removeCount = 0;
    while (removeCount < this.progressSources_.length &&
      this.progressSources_[removeCount][0] <
      this.context_.currentTime - 0.2) {
      removeCount++;
    }
    this.progressSources_.splice(0, removeCount);
  }

  /**
   * Start playing tick / tock progress sounds continuously until
   * explicitly canceled.
   */
  startProgress(): void {
    if (this.progressIntervalID_) {
      this.cancelProgress();
    }

    this.progressSources_ = [];
    this.progressGain_ = 0.5;
    this.progressTime_ = this.context_.currentTime;
    this.generateProgressTickTocks_();
    this.progressIntervalID_ =
      setInterval(() => this.generateProgressTickTocks_(), 1000);
  }

  /** Stop playing any tick / tock progress sounds. */
  cancelProgress(): void {
    if (!this.progressIntervalID_) {
      return;
    }

    for (let i = 0; i < this.progressSources_.length; i++) {
      (this.progressSources_[i][1] as AudioScheduledSourceNode).stop();
    }
    this.progressSources_ = [];

    clearInterval(this.progressIntervalID_);
    this.progressIntervalID_ = null;
  }

  /** Plays sound indicating ChromeVox is loading. */
  onChromeVoxLoading(): void {
    this.play(OggSoundFile.CHROMEVOX_LOADING, { loop: true });
  }

  /**
   * Plays the sound indicating ChromeVox has loaded, and cancels the ChromeVox
   * loading sound.
   */
  onChromeVoxLoaded(): void {
    this.stopLoop(OggSoundFile.CHROMEVOX_LOADING);
    this.play(OggSoundFile.CHROMEVOX_LOADED);
  }

  /**
   * @param {chrome.automation.Rect} rect
   * @param {chrome.automation.Rect} container
   */
  setPositionForRect(rect: chrome.automation.Rect, container: chrome.automation.Rect): void {
    // The horizontal position computed as a percentage relative to its
    // container.
    let x = (rect.left + rect.width / 2) / container.width;

    // Clamp.
    x = Math.min(Math.max(x, 0.0), 1.0);

    // Map to between the negative maximum pan x position and the positive max x
    // pan position.
    x = (2 * x - 1) * MAX_PAN_ABS_X_POSITION;

    this.basePan = x;
  }

  /** Resets panning to default (centered). */
  resetPan(): void {
    this.basePan = CENTER_PAN;
  }

  private multiplierFor_(note: Note | number): number {
    const halfStepsFromA220 = note + HALF_STEPS_TO_C + this.transposeToKey;
    return Math.pow(HALF_STEP, halfStepsFromA220);
  }

  private frequencyFor_(note: Note | number): number {
    return A3_HZ * this.multiplierFor_(note);
  }
}

// Local to module.

/* The list of sound data files to load. */
const WavSoundFile = {
  CONTROL: 'control',
  SKIM: 'skim',
  STATIC: 'static',
};

/* The list of sound data files to load. */
const OggSoundFile = {
  CHROMEVOX_LOADED: 'chromevox_loaded',
  CHROMEVOX_LOADING: 'chromevox_loading',
  INVALID_KEYPRESS: 'invalid_keypress',
  SELECTION: 'selection',
  SELECTION_REVERSE: 'selection_reverse',
};

/** The list of reverb data files to load. */
const Reverb = {
  SMALL_ROOM: 'small_room_2',
};

/** Pitch values for different notes. */
enum Note {
  C2 = -24,
  D_FLAT2 = -23,
  D2 = -22,
  E_FLAT2 = -21,
  E2 = -20,
  F2 = -19,
  G_FLAT2 = -18,
  G2 = -17,
  A_FLAT2 = -16,
  A2 = -15,
  B_FLAT2 = -14,
  B2 = -13,
  C3 = -12,
  D_FLAT3 = -11,
  D3 = -10,
  E_FLAT3 = -9,
  E3 = -8,
  F3 = -7,
  G_FLAT3 = -6,
  G3 = -5,
  A_FLAT3 = -4,
  A3 = -3,
  B_FLAT3 = -2,
  B3 = -1,
  C4 = 0,
  D_FLAT4 = 1,
  D4 = 2,
  E_FLAT4 = 3,
  E4 = 4,
  F4 = 5,
  G_FLAT4 = 6,
  G4 = 7,
  A_FLAT4 = 8,
  A4 = 9,
  B_FLAT4 = 10,
  B4 = 11,
  C5 = 12,
  D_FLAT5 = 13,
  D5 = 14,
  E_FLAT5 = 15,
  E5 = 16,
  F5 = 17,
  G_FLAT5 = 18,
  G5 = 19,
  A_FLAT5 = 20,
  A5 = 21,
  B_FLAT5 = 22,
  B5 = 23,
  C6 = 24,
}

/** The number of half-steps in an octave. */
const HALF_STEPS_PER_OCTAVE = 12;

/**
 * The number of half-steps from the base pitch (A220Hz) to C4 (middle C).
 */
const HALF_STEPS_TO_C = 3;

/**The scale factor for one half-step. */
const HALF_STEP = Math.pow(2.0, 1.0 / HALF_STEPS_PER_OCTAVE);

/**The frequency of the note A3, in Hertz. */
const A3_HZ = 220;

/** The base url for earcon sound resources. */
const BASE_URL = chrome.extension.getURL('chromevox/earcons/');

/**The maximum value to pass to PannerNode.setPosition. */
const MAX_PAN_ABS_X_POSITION = 4;

/**Default (centered) pan position. */
const CENTER_PAN = 0;
