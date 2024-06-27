// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {bindSignal} from '../reactive/local_storage.js';
import {signal} from '../reactive/signal.js';
import {AudioSource} from '../recording_session.js';
import * as localStorage from '../utils/local_storage.js';
import {Infer, z} from '../utils/schema.js';

const settingsSchema = z.object({
  audioSource: z.nativeEnum(AudioSource),
});

type Settings = Infer<typeof settingsSchema>;

const defaultSettings: Settings = {
  audioSource: AudioSource.USER_MEDIA,
};

export const settings = signal(defaultSettings);

/**
 * Initializes settings related states.
 *
 * This binds the state with value from localStorage.
 */
export function init(): void {
  bindSignal(
    settings,
    localStorage.Key.SETTINGS,
    settingsSchema,
    defaultSettings,
  );
}
