// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const INITIAL_ANGULAR_POSITION_MULTIPLIER = 1000;
const BASE_WAVE_AMPLITUDE_FACTOR = 0.53;
const BASE_WAVE_FREQUENCY_FACTOR = 1;
const FIRST_OVERTONE_WAVE_AMPLITUDE_FACTOR = 0.25;
const FIRST_OVERTONE_WAVE_FREQUENCY_FACTOR = 2;
const SECOND_OVERTONE_WAVE_AMPLITUDE_FACTOR = 0.12;
const SECOND_OVERTONE_WAVE_FREQUENCY_FACTOR = 3;

interface WaveParam {
  // Amplitude controls how much the value will change above and below the
  // starting value. For this class, amplitude should be between [-1, 1].
  amplitude: number;
  // Frequency controls how often the value changes per second. A higher
  // frequency makes the value move faster, a lower frequency makes the value
  // move slower.
  frequency: number;
}

function createWaveParam(amplitude: number, frequency: number): WaveParam {
  return {amplitude, frequency};
}

/** Converts frequency to angular frequency (2πf) */
function frequencyToAngularFrequency(frequency: number): number {
  return 2 * Math.PI * frequency;
}

/**
 * Simulates a random "wiggle" motion using the supplied frequency. Values are
 * calculated stepwise, progressing the simulation based on the current time.
 *
 * The calculated values are roughly in the range [-1, 1].
 *
 * For more information see
 * https://www.schoolofmotion.com/blog/wiggle-expression.
 */
export class Wiggle {
  // Randomized constants in pairs of (amplitude, frequency) which make each
  // Wiggle object have a unique path.
  private readonly waveParams = [
    createWaveParam(
        BASE_WAVE_AMPLITUDE_FACTOR * (0.5 + Math.random()),
        BASE_WAVE_FREQUENCY_FACTOR * (0.5 + Math.random()),
        ),
    createWaveParam(
        FIRST_OVERTONE_WAVE_AMPLITUDE_FACTOR * (0.5 + Math.random()),
        FIRST_OVERTONE_WAVE_FREQUENCY_FACTOR * (0.5 + Math.random()),
        ),
    createWaveParam(
        SECOND_OVERTONE_WAVE_AMPLITUDE_FACTOR * (0.5 + Math.random()),
        SECOND_OVERTONE_WAVE_FREQUENCY_FACTOR * (0.5 + Math.random()),
        ),
  ];

  /** Wiggle angular frequency (2πf) */
  private angularFrequency: number;
  /** The internal position of the simulation that is stepped forward */
  private angularPosition: number;
  /** Time in seconds of previous calculation */
  private previousTimeSeconds?: number;
  /** Value of the previous calculated wiggle simulation value. */
  private previousWiggleValue: number;

  constructor(
      frequency: number,
  ) {
    this.angularFrequency = frequencyToAngularFrequency(frequency);
    this.angularPosition = Math.random() * INITIAL_ANGULAR_POSITION_MULTIPLIER;
    // If there was no previous wiggle value (as in the case of entering through
    // the image context menu item), then it is possible for all of the circles
    // to overlap causing their simulated gaussian blur to become more apparent
    // at the gradient color stops. Calculate an initial wiggle value to prevent
    // this.
    this.previousWiggleValue = this.calculateNext(0);
  }

  getPreviousWiggleValue(): number {
    return this.previousWiggleValue;
  }

  setFrequency(frequency: number) {
    this.angularFrequency = frequencyToAngularFrequency(frequency);
  }

  /**
   * Calculates the state of the Wiggle simulation for the current time
   *
   * @param timeSeconds Current simulation time in seconds
   */
  calculateNext(timeSeconds: number): number {
    if (!this.previousTimeSeconds) {
      this.previousTimeSeconds = timeSeconds;
    }
    this.angularPosition +=
        (timeSeconds - this.previousTimeSeconds) * this.angularFrequency;
    this.previousTimeSeconds = timeSeconds;

    let wiggle = 0;
    for (const {amplitude, frequency} of this.waveParams) {
      wiggle += amplitude * Math.sin(frequency * this.angularPosition);
    }
    this.previousWiggleValue = wiggle;
    return wiggle;
  }
}
