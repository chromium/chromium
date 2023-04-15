// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is the low-level class that generates ChromeVox's
 * earcons. It's designed to be self-contained and not depend on the
 * rest of the code.
 */

import {EarconId} from '../common/earcon_id.js';

/** EarconEngine generates ChromeVox's earcons using the web audio API. */
export class EarconEngine {
  constructor() {
    // Public control parameters. All of these are meant to be adjustable.

    /** @public {number} The output volume, as an amplification factor. */
    this.outputVolume = 1.0;

    /**
     * As notated below, all pitches are in the key of C. This can be set to
     * transpose the key from C to another pitch.
     * @public {!Note}
     */
    this.transposeToKey = Note.B_FLAT3;

    /** @public {number} The click volume, as an amplification factor. */
    this.clickVolume = 0.4;

    /**
     * @public {number} The volume of the static sound, as an
     * amplification factor.
     */
    this.staticVolume = 0.2;

    /** @public {number} The base delay for repeated sounds, in seconds. */
    this.baseDelay = 0.045;

    /** @public {number} The base stereo panning, from -1 to 1. */
    this.basePan = CENTER_PAN;

    /** @public {number} The base reverb level as an amplification factor. */
    this.baseReverb = 0.4;

    /** @public {!Reverb} The choice of the reverb impulse response to use. */
    this.reverbSound = Reverb.SMALL_ROOM;

    /** @public {!Note} The base pitch for the 'wrap' sound. */
    this.wrapPitch = Note.G_FLAT3;

    /** @public {!Note} The base pitch for the 'alert' sound. */
    this.alertPitch = Note.G_FLAT3;

    /** @public {!Note} The default pitch. */
    this.defaultPitch = Note.G3;

    /** @public {string} The choice of base sound for most controls. */
    this.controlSound = WavSoundFile.CONTROL;

    /**
     * @public {number} The delay between sounds in the on/off sweep effect,
     * in seconds.
     */
    this.sweepDelay = 0.045;

    /**
     * @public {number} The delay between echos in the on/off sweep, in seconds.
     */
    this.sweepEchoDelay = 0.15;

    /** @public {number} The number of echos in the on/off sweep. */
    this.sweepEchoCount = 3;

    /** @public {!Note} The pitch offset of the on/off sweep. */
    this.sweepPitch = Note.C3;

    /**
     * @public {number} The final gain of the progress sound, as an
     * amplification factor.
     */
    this.progressFinalGain = 0.05;

    /** @public {number} The multiplicative decay rate of the progress ticks. */
    this.progressGain_Decay = 0.7;

    // Private variables.

    /** @private {AudioContext} The audio context. */
    this.context_ = new AudioContext();

    /** @private {?ConvolverNode} The reverb node, lazily initialized. */
    this.reverbConvolver_ = null;

    /**
     * @private {Object<string, AudioBuffer>} A map between the name of an
     *     audio data file and its loaded AudioBuffer.
     */
    this.buffers_ = {};

    /**
     * The source audio nodes for queued tick / tocks for progress.
     * Kept around so they can be canceled.
     *
     * @private {Array<Array<AudioNode>>}
     */
    this.progressSources_ = [];

    /** @private {number} The current gain for progress sounds. */
    this.progressGain_ = 1.0;

    /** @private {?number} The current time for progress sounds. */
    this.progressTime_ = this.context_.currentTime;

    /** @private {?number} The setInterval ID for progress sounds. */
    this.progressIntervalID_ = null;

    /** @private {boolean} */
    this.persistProgressTicks_ = false;

    /**
     * Maps a earcon name to the last source input audio for that
     * earcon.
     * @private {!Object<!EarconId, !AudioNode|undefined>}
     */
    this.lastEarconSources_ = {};

    /** @private {!EarconId|undefined} */
    this.currentTrackedEarcon_;

    // Initialization: load the base sound data files asynchronously.
    Object.values(WavSoundFile)
        .concat(Object.values(Reverb))
        .forEach(sound => this.loadSound(sound, `${BASE_URL}${sound}.wav`));
    Object.values(OggSoundFile)
        .forEach(sound => this.loadSound(sound, `${BASE_URL}${sound}.ogg`));
  }

  /**
   * A high-level way to ask the engine to play a specific earcon.
   * @param {!EarconId} earcon The earcon to play.
   */
  playEarcon(earcon) {
    // These earcons are not tracked by the engine via their audio sources.
    switch (earcon) {
      case EarconId.CHROMEVOX_LOADED:
        this.cancelProgressPersistent();
        return;
      case EarconId.CHROMEVOX_LOADING:
        this.startProgressPersistent();
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
        this.onWrap();
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
    if (source !== undefined) {
      source.onended = () => {
        delete this.lastEarconSources_[earcon];
      };
    }
  }

  /**
   * Fetches a sound asynchronously and loads its data into an AudioBuffer.
   *
   * @param {string} name The name of the sound to load.
   * @param {string} url The url where the sound should be fetched from.
   */
  loadSound(name, url) {
    const request = new XMLHttpRequest();
    request.open('GET', url, true);
    request.responseType = 'arraybuffer';

    // Decode asynchronously.
    request.onload = () => {
      this.context_.decodeAudioData(
          /** @type {!ArrayBuffer} */ (request.response),
          buffer => this.buffers_[name] = buffer);
    };
    request.send();
  }

  /**
   * Return an AudioNode containing the final processing that all
   * sounds go through: output volume / gain, panning, and reverb.
   * The chain is hooked up to the destination automatically, so you
   * just need to connect your source to the return value from this
   * method.
   *
   * @param {{gain: (number | undefined),
   *          pan: (number | undefined),
   *          reverb: (number | undefined)}} properties
   *     An object where you can override the default
   *     gain, pan, and reverb, otherwise these are taken from
   *     outputVolume, basePan, and baseReverb.
   * @return {!AudioNode} The filters to be applied to all sounds, connected
   *     to the destination node.
   */
  createCommonFilters(properties) {
    let gain = this.outputVolume;
    if (properties.gain) {
      gain *= properties.gain;
    }
    const gainNode = this.context_.createGain();
    gainNode.gain.value = gain;
    const first = gainNode;
    let last = gainNode;

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
   * @param {string} sound The name of the sound to play. It must already
   *     be loaded in a buffer.
   * @param {{pitch: (number | undefined),
   *          time: (number | undefined),
   *          gain: (number | undefined),
   *          pan: (number | undefined),
   *          reverb: (number | undefined)}=} opt_properties
   *     An object where you can override the default pitch, gain, pan,
   *     and reverb.
   * @return {AudioBufferSourceNode} The source node, so you can stop it
   *     or set event handlers on it.
   */
  play(sound, opt_properties = {}) {
    const source = this.context_.createBufferSource();
    source.buffer = this.buffers_[sound];

    const pitch = opt_properties.pitch ?? this.defaultPitch;
    // Changes the playback rate of the sample – which also changes the pitch.
    source.playbackRate.value = this.multiplierFor_(pitch);

    const destination = this.createCommonFilters(opt_properties);
    source.connect(destination);
    if (this.currentTrackedEarcon_) {
      this.lastEarconSources_[this.currentTrackedEarcon_] = source;
    }
    if (opt_properties.time) {
      source.start(this.context_.currentTime + opt_properties.time);
    } else {
      source.start(this.context_.currentTime);
    }

    return source;
  }

  /** Play the static sound. */
  onStatic() {
    this.play(WavSoundFile.STATIC, {gain: this.staticVolume});
  }

  /** Play the link sound. */
  onLink() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume});
    this.play(this.controlSound, {pitch: Note.G4});
  }

  /** Play the button sound. */
  onButton() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume});
    this.play(this.controlSound);
  }

  /** Play the text field sound. */
  onTextField() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume});
    this.play(
        WavSoundFile.STATIC,
        {time: this.baseDelay * 1.5, gain: this.clickVolume * 0.5});
    this.play(this.controlSound, {pitch: Note.B3});
    this.play(
        this.controlSound,
        {pitch: Note.B3, time: this.baseDelay * 1.5, gain: 0.5});
  }

  /** Play the pop up button sound. */
  onPopUpButton() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume});

    this.play(this.controlSound);
    this.play(
        this.controlSound,
        {time: this.baseDelay * 3, gain: 0.2, pitch: Note.G4});
    this.play(
        this.controlSound,
        {time: this.baseDelay * 4.5, gain: 0.2, pitch: Note.G4});
  }

  /** Play the check on sound. */
  onCheckOn() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume});
    this.play(this.controlSound, {pitch: Note.D3});
    this.play(this.controlSound, {pitch: Note.D4, time: this.baseDelay * 2});
  }

  /** Play the check off sound. */
  onCheckOff() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume});
    this.play(this.controlSound, {pitch: Note.D4});
    this.play(this.controlSound, {pitch: Note.D3, time: this.baseDelay * 2});
  }

  /** Play the smart sticky mode on sound. */
  onSmartStickyModeOn() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume * 0.5});
    this.play(this.controlSound, {pitch: Note.D4});
  }

  /** Play the smart sticky mode off sound. */
  onSmartStickyModeOff() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume * 0.5});
    this.play(this.controlSound, {pitch: Note.D3});
  }

  /** Play the select control sound. */
  onSelect() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume});
    this.play(this.controlSound);
    this.play(this.controlSound, {time: this.baseDelay});
    this.play(this.controlSound, {time: this.baseDelay * 2});
  }

  /** Play the slider sound. */
  onSlider() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume});
    this.play(this.controlSound);
    this.play(
        this.controlSound, {time: this.baseDelay, gain: 0.5, pitch: Note.A3});
    this.play(
        this.controlSound,
        {time: this.baseDelay * 2, gain: 0.25, pitch: Note.B3});
    this.play(
        this.controlSound,
        {time: this.baseDelay * 3, gain: 0.125, pitch: Note.D_FLAT4});
    this.play(
        this.controlSound,
        {time: this.baseDelay * 4, gain: 0.0625, pitch: Note.E_FLAT4});
  }

  /** Play the skim sound. */
  onSkim() {
    this.play(WavSoundFile.SKIM);
  }

  /** Play the selection sound. */
  onSelection() {
    this.play(OggSoundFile.SELECTION);
  }

  /** Play the selection reverse sound. */
  onSelectionReverse() {
    this.play(OggSoundFile.SELECTION_REVERSE);
  }

  onNoPointerAnchor() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume * 0.2});
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
   * @param {{gain: number,
   *          freq: number,
   *          endFreq: (number | undefined),
   *          time: (number | undefined),
   *          overtones: (number | undefined),
   *          overtoneFactor: (number | undefined),
   *          dur: (number | undefined),
   *          attack: (number | undefined),
   *          decay: (number | undefined),
   *          pan: (number | undefined),
   *          reverb: (number | undefined)}} properties
   *     An object containing the properties that can be used to
   *     control the sound, as described above.
   */
  generateSinusoidal(properties) {
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
    for (let i = 0; i < properties.overtones; i++) {
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
            this.context_.currentTime + properties.dur);
      }

      osc.start(this.context_.currentTime + time);
      osc.stop(this.context_.currentTime + time + properties.dur);

      const gainNode = this.context_.createGain();
      gainNode.gain.value = gain;
      osc.connect(gainNode);
      gainNode.connect(envelopeNode);

      gain *= properties.overtoneFactor;
    }

    // Shape the overall sound by an envelope based on the attack and
    // decay times.
    envelopeNode.gain.setValueAtTime(0, this.context_.currentTime + time);
    envelopeNode.gain.linearRampToValueAtTime(
        1, this.context_.currentTime + time + properties.attack);
    envelopeNode.gain.setValueAtTime(
        1,
        this.context_.currentTime + time + properties.dur - properties.decay);
    envelopeNode.gain.linearRampToValueAtTime(
        0, this.context_.currentTime + time + properties.dur);

    // Route everything through the common filters like reverb at the end.
    const destination = this.createCommonFilters({});
    envelopeNode.connect(destination);
  }

  /**
   * Play a sweep over a bunch of notes in a scale, with an echo,
   * for the ChromeVox on or off sounds.
   *
   * @param {boolean} reverse Whether to play in the reverse direction.
   */
  onChromeVoxSweep(reverse) {
    const pitches = [
      Note.C2,
      Note.D3,
      Note.G3,
      Note.C3,
      Note.D4,
      Note.G4,
      Note.C4,
      Note.D5,
      Note.G5,
    ];

    if (reverse) {
      pitches.reverse();
    }

    const attack = 0.015;
    const dur = pitches.length * this.sweepDelay;

    const destination = this.createCommonFilters({reverb: 2.0});
    for (let k = 0; k < this.sweepEchoCount; k++) {
      const envelopeNode = this.context_.createGain();
      const startTime = this.context_.currentTime + this.sweepEchoDelay * k;
      const sweepGain = Math.pow(0.3, k);
      const overtones = 2;
      let overtoneGain = sweepGain;
      for (let i = 0; i < overtones; i++) {
        const osc = this.context_.createOscillator();
        osc.start(startTime);
        osc.stop(startTime + dur);

        const gainNode = this.context_.createGain();
        osc.connect(gainNode);
        gainNode.connect(envelopeNode);

        for (let j = 0; j < pitches.length; j++) {
          let freqDecay;
          if (reverse) {
            freqDecay = Math.pow(0.75, pitches.length - j);
          } else {
            freqDecay = Math.pow(0.75, j);
          }
          const gain = overtoneGain * freqDecay;
          const pitch = pitches[j] + this.sweepPitch;
          const freq = (i + 1) * this.frequencyFor_(pitch);
          if (j === 0) {
            osc.frequency.setValueAtTime(freq, startTime);
            gainNode.gain.setValueAtTime(gain, startTime);
          } else {
            osc.frequency.exponentialRampToValueAtTime(
                freq, startTime + j * this.sweepDelay);
            gainNode.gain.linearRampToValueAtTime(
                gain, startTime + j * this.sweepDelay);
          }
          osc.frequency.setValueAtTime(
              freq, startTime + j * this.sweepDelay + this.sweepDelay - attack);
        }

        overtoneGain *= 0.1 + 0.2 * k;
      }

      envelopeNode.gain.setValueAtTime(0, startTime);
      envelopeNode.gain.linearRampToValueAtTime(1, startTime + this.sweepDelay);
      envelopeNode.gain.setValueAtTime(1, startTime + dur - attack * 2);
      envelopeNode.gain.linearRampToValueAtTime(0, startTime + dur);
      envelopeNode.connect(destination);
    }
  }

  /** Play the "ChromeVox On" sound. */
  onChromeVoxOn() {
    this.onChromeVoxSweep(false);
  }

  /** Play the "ChromeVox Off" sound. */
  onChromeVoxOff() {
    this.onChromeVoxSweep(true);
  }

  /** Play an alert sound. */
  onAlert() {
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
  onWrap() {
    this.play(WavSoundFile.STATIC, {gain: this.clickVolume * 0.3});
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
   * @private
   */
  generateProgressTickTocks_() {
    while (this.progressTime_ < this.context_.currentTime + 3.0) {
      let t = this.progressTime_ - this.context_.currentTime;
      this.progressSources_.push([
        this.progressTime_,
        this.play(
            WavSoundFile.STATIC, {gain: 0.5 * this.progressGain_, time: t}),
      ]);
      this.progressSources_.push([
        this.progressTime_,
        this.play(
            this.controlSound,
            {pitch: Note.E_FLAT5, time: t, gain: this.progressGain_}),
      ]);

      if (this.progressGain_ > this.progressFinalGain) {
        this.progressGain_ *= this.progressGain_Decay;
      }
      t += 0.5;

      this.progressSources_.push([
        this.progressTime_,
        this.play(
            WavSoundFile.STATIC, {gain: 0.5 * this.progressGain_, time: t}),
      ]);
      this.progressSources_.push([
        this.progressTime_,
        this.play(
            this.controlSound,
            {pitch: Note.E_FLAT4, time: t, gain: this.progressGain_}),
      ]);

      if (this.progressGain_ > this.progressFinalGain) {
        this.progressGain_ *= this.progressGain_Decay;
      }

      this.progressTime_ += 1.0;
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
  startProgress() {
    if (this.persistProgressTicks_) {
      return;
    }

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
  cancelProgress() {
    if (this.persistProgressTicks_) {
      return;
    }
    if (!this.progressIntervalID_) {
      return;
    }

    for (let i = 0; i < this.progressSources_.length; i++) {
      this.progressSources_[i][1].stop();
    }
    this.progressSources_ = [];

    clearInterval(this.progressIntervalID_);
    this.progressIntervalID_ = null;
  }

  /**
   * Similar to the non-persistent variant above, but does not allow for
   * cancellation by other calls to startProgress*.
   */
  startProgressPersistent() {
    if (this.persistProgressTicks_) {
      return;
    }
    this.startProgress();
    this.persistProgressTicks_ = true;
  }

  /**
   * Similar to the non-persistent variant above, but does not allow for
   * cancellation by other calls to cancelProgress*.
   */
  cancelProgressPersistent() {
    if (!this.persistProgressTicks_) {
      return;
    }
    this.persistProgressTicks_ = false;
    this.cancelProgress();
  }

  /**
   * @param {chrome.automation.Rect} rect
   * @param {chrome.automation.Rect} container
   */
  setPositionForRect(rect, container) {
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
  resetPan() {
    this.basePan = CENTER_PAN;
  }

  /**
   * @param {!Note|number} note
   * @return {number}
   * @private
   */
  multiplierFor_(note) {
    const halfStepsFromA220 = note + HALF_STEPS_TO_C + this.transposeToKey;
    return Math.pow(HALF_STEP, halfStepsFromA220);
  }

  /**
   * @param {!Note|number} note
   * @return {number}
   * @private
   */
  frequencyFor_(note) {
    return A3_HZ * this.multiplierFor_(note);
  }
}

// Local to module.

/* @enum {string} The list of sound data files to load. */
const WavSoundFile = {
  CONTROL: 'control',
  SKIM: 'skim',
  STATIC: 'static',
};

/* @enum {string} The list of sound data files to load. */
const OggSoundFile = {
  CHROMEVOX_LOADED: 'chromevox_loaded',
  CHROMEVOX_LOADING: 'chromevox_loading',
  INVALID_KEYPRESS: 'invalid_keypress',
  SELECTION: 'selection',
  SELECTION_REVERSE: 'selection_reverse',
};

/** @enum {string} The list of reverb data files to load. */
const Reverb = {
  SMALL_ROOM: 'small_room_2',
};

/** @enum {number} Pitch values for different notes. */
const Note = {
  C2: -24,
  D_FLAT2: -23,
  D2: -22,
  E_FLAT2: -21,
  E2: -20,
  F2: -19,
  G_FLAT2: -18,
  G2: -17,
  A_FLAT2: -16,
  A2: -15,
  B_FLAT2: -14,
  B2: -13,
  C3: -12,
  D_FLAT3: -11,
  D3: -10,
  E_FLAT3: -9,
  E3: -8,
  F3: -7,
  G_FLAT3: -6,
  G3: -5,
  A_FLAT3: -4,
  A3: -3,
  B_FLAT3: -2,
  B3: -1,
  C4: 0,
  D_FLAT4: 1,
  D4: 2,
  E_FLAT4: 3,
  E4: 4,
  F4: 5,
  G_FLAT4: 6,
  G4: 7,
  A_FLAT4: 8,
  A4: 9,
  B_FLAT4: 10,
  B4: 11,
  C5: 12,
  D_FLAT5: 13,
  D5: 14,
  E_FLAT5: 15,
  E5: 16,
  F5: 17,
  G_FLAT5: 18,
  G5: 19,
  A_FLAT5: 20,
  A5: 21,
  B_FLAT5: 22,
  B5: 23,
  C6: 24,
};

/** @const {number} The number of half-steps in an octave. */
const HALF_STEPS_PER_OCTAVE = 12;

/**
 * The number of half-steps from the base pitch (A220Hz) to C4 (middle C).
 * @const {number}
 */
const HALF_STEPS_TO_C = 3;

/** @const {number} The scale factor for one half-step. */
const HALF_STEP = Math.pow(2.0, 1.0 / HALF_STEPS_PER_OCTAVE);

/** @const {number} The frequency of the note A3, in Hertz. */
const A3_HZ = 220;

/** @const {string} The base url for earcon sound resources. */
const BASE_URL = chrome.extension.getURL('chromevox/earcons/');

/** @const {number} The maximum value to pass to PannerNode.setPosition. */
const MAX_PAN_ABS_X_POSITION = 4;

/** @const {number} Default (centered) pan position. */
const CENTER_PAN = 0;
