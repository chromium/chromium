// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is the low-level class that generates ChromeVox's
 * earcons. It's designed to be self-contained and not depend on the
 * rest of the code.
 */

import {Earcon} from '../common/abstract_earcons.js';

/** EarconEngine generates ChromeVox's earcons using the web audio API. */
export class EarconEngine {
  constructor() {
    // Public control parameters. All of these are meant to be adjustable.

    /** @public {number} The output volume, as an amplification factor. */
    this.outputVolume = 1.0;

    /** @public {number} The base relative pitch adjustment, in half-steps. */
    this.basePitch = -4;

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
    this.basePan = EarconEngine.CENTER_PAN_;

    /** @public {number} The base reverb level as an amplification factor. */
    this.baseReverb = 0.4;

    /**
     * @public {string} The choice of the reverb impulse response to use.
     * Must be one of the strings from EarconEngine.REVERBS.
     */
    this.reverbSound = 'small_room_2';

    /** @public {number} The base pitch for the 'wrap' sound in half-steps. */
    this.wrapPitch = 0;

    /** @public {number} The base pitch for the 'alert' sound in half-steps. */
    this.alertPitch = 0;

    /** @public {string} The choice of base sound for most controls. */
    this.controlSound = 'control';

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

    /** @public {number} The pitch offset of the on/off sweep, in half-steps. */
    this.sweepPitch = -7;

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
     * @private {!Object<!Earcon, !AudioNode|undefined>}
     */
    this.lastEarconSources_ = {};

    /** @private {!Earcon|undefined} */
    this.currentTrackedEarcon_;

    // Initialization: load the base sound data files asynchronously.
    const allSoundFilesToLoad =
        EarconEngine.SOUNDS.concat(EarconEngine.REVERBS);
    allSoundFilesToLoad.forEach(sound => {
      const url = `${EarconEngine.BASE_URL}${sound}.wav`;
      this.loadSound(sound, url);
    });
  }

  /**
   * A high-level way to ask the engine to play a specific earcon.
   * @param {!Earcon} earcon The earcon to play.
   */
  playEarcon(earcon) {
    // These earcons are not tracked by the engine via their audio sources.
    switch (earcon) {
      case Earcon.CHROMEVOX_LOADED:
        this.cancelProgressPersistent();
        return;
      case Earcon.CHROMEVOX_LOADING:
        this.startProgressPersistent();
        return;
      case Earcon.PAGE_FINISH_LOADING:
        this.cancelProgress();
        return;
      case Earcon.PAGE_START_LOADING:
        this.startProgress();
        return;
      case Earcon.POP_UP_BUTTON:
        this.onPopUpButton();
        return;

      // These had earcons in previous versions of ChromeVox but
      // they're currently unused / unassigned.
      case Earcon.LIST_ITEM:
      case Earcon.LONG_DESC:
      case Earcon.MATH:
      case Earcon.OBJECT_CLOSE:
      case Earcon.OBJECT_ENTER:
      case Earcon.OBJECT_EXIT:
      case Earcon.OBJECT_OPEN:
      case Earcon.OBJECT_SELECT:
      case Earcon.RECOVER_FOCUS:
        return;
    }

    // These earcons are tracked by the engine via their audio sources.
    if (this.lastEarconSources_[earcon] !== undefined) {
      // Playback of |earcon| is in progress.
      return;
    }

    this.currentTrackedEarcon_ = earcon;
    switch (earcon) {
      case Earcon.ALERT_MODAL:
      case Earcon.ALERT_NONMODAL:
        this.onAlert();
        break;
      case Earcon.BUTTON:
        this.onButton();
        break;
      case Earcon.CHECK_OFF:
        this.onCheckOff();
        break;
      case Earcon.CHECK_ON:
        this.onCheckOn();
        break;
      case Earcon.EDITABLE_TEXT:
        this.onTextField();
        break;
      case Earcon.INVALID_KEYPRESS:
        this.onWrap();
        break;
      case Earcon.LINK:
        this.onLink();
        break;
      case Earcon.LISTBOX:
        this.onSelect();
        break;
      case Earcon.SELECTION:
        this.onSelection();
        break;
      case Earcon.SELECTION_REVERSE:
        this.onSelectionReverse();
        break;
      case Earcon.SKIP:
        this.onSkim();
        break;
      case Earcon.SLIDER:
        this.onSlider();
        break;
      case Earcon.SMART_STICKY_MODE_OFF:
        this.onSmartStickyModeOff();
        break;
      case Earcon.SMART_STICKY_MODE_ON:
        this.onSmartStickyModeOn();
        break;
      case Earcon.NO_POINTER_ANCHOR:
        this.onNoPointerAnchor();
        break;
      case Earcon.WRAP:
      case Earcon.WRAP_EDGE:
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
    request.onload = (function() {
                       this.context_.decodeAudioData(
                           /** @type {!ArrayBuffer} */ (request.response),
                           (function(buffer) {
                             this.buffers_[name] = buffer;
                           }).bind(this));
                     }).bind(this);
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

    let pan = this.basePan;
    if (properties.pan !== undefined) {
      pan = properties.pan;
    }
    if (pan !== 0) {
      const panNode = this.context_.createPanner();
      panNode.setPosition(pan, 0, 0);
      panNode.setOrientation(0, 0, 1);
      last.connect(panNode);
      last = panNode;
    }

    let reverb = this.baseReverb;
    if (properties.reverb !== undefined) {
      reverb = properties.reverb;
    }
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
  play(sound, opt_properties) {
    const source = this.context_.createBufferSource();
    source.buffer = this.buffers_[sound];

    if (!opt_properties) {
      // This typecast looks silly, but the Closure compiler doesn't support
      // optional fields in record types very well so this is the shortest hack.
      opt_properties = /** @type {undefined} */ ({});
    }

    let pitch = this.basePitch;
    if (opt_properties.pitch) {
      pitch += opt_properties.pitch;
    }
    if (pitch !== 0) {
      source.playbackRate.value = Math.pow(EarconEngine.HALF_STEP, pitch);
    }

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
    this.play('static', {gain: this.staticVolume});
  }

  /** Play the link sound. */
  onLink() {
    this.play('static', {gain: this.clickVolume});
    this.play(this.controlSound, {pitch: 12});
  }

  /** Play the button sound. */
  onButton() {
    this.play('static', {gain: this.clickVolume});
    this.play(this.controlSound);
  }

  /** Play the text field sound. */
  onTextField() {
    this.play('static', {gain: this.clickVolume});
    this.play(
        'static', {time: this.baseDelay * 1.5, gain: this.clickVolume * 0.5});
    this.play(this.controlSound, {pitch: 4});
    this.play(
        this.controlSound, {pitch: 4, time: this.baseDelay * 1.5, gain: 0.5});
  }

  /** Play the pop up button sound. */
  onPopUpButton() {
    this.play('static', {gain: this.clickVolume});

    this.play(this.controlSound);
    this.play(
        this.controlSound, {time: this.baseDelay * 3, gain: 0.2, pitch: 12});
    this.play(
        this.controlSound, {time: this.baseDelay * 4.5, gain: 0.2, pitch: 12});
  }

  /** Play the check on sound. */
  onCheckOn() {
    this.play('static', {gain: this.clickVolume});
    this.play(this.controlSound, {pitch: -5});
    this.play(this.controlSound, {pitch: 7, time: this.baseDelay * 2});
  }

  /** Play the check off sound. */
  onCheckOff() {
    this.play('static', {gain: this.clickVolume});
    this.play(this.controlSound, {pitch: 7});
    this.play(this.controlSound, {pitch: -5, time: this.baseDelay * 2});
  }

  /** Play the smart sticky mode on sound. */
  onSmartStickyModeOn() {
    this.play('static', {gain: this.clickVolume * 0.5});
    this.play(this.controlSound, {pitch: 7});
  }

  /** Play the smart sticky mode off sound. */
  onSmartStickyModeOff() {
    this.play('static', {gain: this.clickVolume * 0.5});
    this.play(this.controlSound, {pitch: -5});
  }

  /** Play the select control sound. */
  onSelect() {
    this.play('static', {gain: this.clickVolume});
    this.play(this.controlSound);
    this.play(this.controlSound, {time: this.baseDelay});
    this.play(this.controlSound, {time: this.baseDelay * 2});
  }

  /** Play the slider sound. */
  onSlider() {
    this.play('static', {gain: this.clickVolume});
    this.play(this.controlSound);
    this.play(this.controlSound, {time: this.baseDelay, gain: 0.5, pitch: 2});
    this.play(
        this.controlSound, {time: this.baseDelay * 2, gain: 0.25, pitch: 4});
    this.play(
        this.controlSound, {time: this.baseDelay * 3, gain: 0.125, pitch: 6});
    this.play(
        this.controlSound, {time: this.baseDelay * 4, gain: 0.0625, pitch: 8});
  }

  /** Play the skim sound. */
  onSkim() {
    this.play('skim');
  }

  /** Play the selection sound. */
  onSelection() {
    this.play('selection');
  }

  /** Play the selection reverse sound. */
  onSelectionReverse() {
    this.play('selection_reverse');
  }

  onNoPointerAnchor() {
    this.play('static', {gain: this.clickVolume * 0.2});
    const freq1 = 220 * Math.pow(EarconEngine.HALF_STEP, 13);
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

    let time = properties.time;
    if (time === undefined) {
      time = 0;
    }

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
    const pitches = [-7, -5, 0, 5, 7, 12, 17, 19, 24];

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
          const freq = (i + 1) * 220 *
              Math.pow(EarconEngine.HALF_STEP, pitches[j] + this.sweepPitch);
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
    const freq1 = 220 * Math.pow(EarconEngine.HALF_STEP, this.alertPitch - 2);
    const freq2 = 220 * Math.pow(EarconEngine.HALF_STEP, this.alertPitch - 3);
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
    this.play('static', {gain: this.clickVolume * 0.3});
    const freq1 = 220 * Math.pow(EarconEngine.HALF_STEP, this.wrapPitch - 8);
    const freq2 = 220 * Math.pow(EarconEngine.HALF_STEP, this.wrapPitch + 8);
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
        this.play('static', {gain: 0.5 * this.progressGain_, time: t}),
      ]);
      this.progressSources_.push([
        this.progressTime_,
        this.play(
            this.controlSound, {pitch: 20, time: t, gain: this.progressGain_}),
      ]);

      if (this.progressGain_ > this.progressFinalGain) {
        this.progressGain_ *= this.progressGain_Decay;
      }
      t += 0.5;

      this.progressSources_.push([
        this.progressTime_,
        this.play('static', {gain: 0.5 * this.progressGain_, time: t}),
      ]);
      this.progressSources_.push([
        this.progressTime_,
        this.play(
            this.controlSound, {pitch: 8, time: t, gain: this.progressGain_}),
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
        setInterval(this.generateProgressTickTocks_.bind(this), 1000);
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
    x = (2 * x - 1) * EarconEngine.MAX_PAN_ABS_X_POSITION;

    this.basePan = x;
  }

  /** Resets panning to default (centered). */
  resetPan() {
    this.basePan = EarconEngine.CENTER_PAN_;
  }
}

/* @const {Array<string>} The list of sound data files to load. */
EarconEngine.SOUNDS =
    ['control', 'selection', 'selection_reverse', 'skim', 'static'];

/** @const {Array<string>} The list of reverb data files to load. */
EarconEngine.REVERBS = ['small_room_2'];

/** @const {number} The scale factor for one half-step. */
EarconEngine.HALF_STEP = Math.pow(2.0, 1.0 / 12.0);

/** @const {string} The base url for earcon sound resources. */
EarconEngine.BASE_URL = chrome.extension.getURL('chromevox/earcons/');

/** The maximum value to pass to PannerNode.setPosition. */
EarconEngine.MAX_PAN_ABS_X_POSITION = 4;

/** @const {number} Default (centered) pan position. */
EarconEngine.CENTER_PAN_ = 0;
