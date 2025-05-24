// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  ModelExecutionError,
  ModelLoadError,
} from '../../core/on_device_model/types.js';
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
  // Force language picker or dropdown display even if there's only one
  // language option.
  forceLanguageSelection: z.boolean(),
  forceTheme: z.optional(z.nativeEnum(ColorTheme)),
  // Simulate GenAi model execution error.
  forceGenAiModelExecutionError:
    z.withDefault(z.nullable(z.nativeEnum(ModelExecutionError)), null),
  // Simulate GenAi model load error.
  forceGenAiModelLoadError:
    z.withDefault(z.nullable(z.nativeEnum(ModelLoadError)), null),
  // Simulate first time GenAi model installation cross session.
  genAiInstalled: z.withDefault(z.boolean(), false),
  // Simulate first time soda installation cross session.
  sodaInstalled: z.boolean(),
  canUseSpeakerLabel: z.boolean(),
  canCaptureSystemAudioWithLoopback: z.boolean(),
});

type DevSettings = Infer<typeof devSettingsSchema>;

const defaultSettings: DevSettings = {
  forceLanguageSelection: false,
  forceTheme: ColorTheme.LIGHT,
  forceGenAiModelExecutionError: null,
  forceGenAiModelLoadError: null,
  genAiInstalled: false,
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
