// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {bindSignal} from '../../core/reactive/local_storage.js';
import {effect, signal} from '../../core/reactive/signal.js';
import * as localStorage from '../../core/utils/local_storage.js';
import {Infer, z} from '../../core/utils/schema.js';

export enum ColorTheme {
  SYSTEM = 'system',
  LIGHT = 'light',
  DARK = 'dark',
}

export const devSettingsSchema = z.object({
  forceTheme: z.optional(z.nativeEnum(ColorTheme)),
  // Simulate first time soda installation cross session.
  sodaInstalled: z.boolean(),
  canUseSpeakerLabel: z.boolean(),
  canCaptureSystemAudioWithLoopback: z.boolean(),
  // TODO(pihsun): Persist summary model installation progress.
});

type DevSettings = Infer<typeof devSettingsSchema>;

const defaultSettings: DevSettings = {
  forceTheme: ColorTheme.LIGHT,
  sodaInstalled: false,
  canUseSpeakerLabel: true,
  canCaptureSystemAudioWithLoopback: true,
};

export const devSettings = signal(defaultSettings);

const DEV_CSS = `
:root.force-dark {
  --dark-theme: 1;
}
:root.force-light {
  --dark-theme: 0;
}
`;

/**
 * Initializes settings related states.
 *
 * This binds the state with value from localStorage, and inserts needed style
 * for force theme override to work.
 */
export function init(): void {
  const style = document.createElement('style');
  style.textContent = DEV_CSS;
  document.head.appendChild(style);

  bindSignal(
    devSettings,
    localStorage.Key.DEV_SETTINGS,
    devSettingsSchema,
    defaultSettings,
  );

  effect(() => {
    document.documentElement.classList.toggle(
      'force-dark',
      devSettings.value.forceTheme === ColorTheme.DARK,
    );
    document.documentElement.classList.toggle(
      'force-light',
      devSettings.value.forceTheme === ColorTheme.LIGHT,
    );
  });
}
