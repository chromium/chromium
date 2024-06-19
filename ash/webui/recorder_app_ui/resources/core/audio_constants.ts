// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The scaling of the power used for visualization.
export const POWER_SCALE_FACTOR = 256;

// TODO(pihsun): The sample rate is fixed at 16K in SodaConfig in
// recorder_app_ui.cc now. Consider make this configurable / increase this for
// better quality audio.
export const SAMPLE_RATE = 16000;

// The window size of samples that is processed by the worklets at one time.
// This is a fixed value defined in Web Audio API that can't be configured.
export const SAMPLES_PER_SLICE = 128;
