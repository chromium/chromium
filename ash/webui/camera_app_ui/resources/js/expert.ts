// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as localStorage from './models/local_storage.js';
import * as state from './state.js';
import {LocalStorageKey, PerfInformation} from './type.js';

export enum ExpertOption {
  CUSTOM_VIDEO_PARAMETERS = 'custom-video-parameters',
  ENABLE_FPS_PICKER_FOR_BUILTIN = 'enable-fps-picker-for-builtin',
  ENABLE_FULL_SIZED_VIDEO_SNAPSHOT = 'enable-full-sized-video-snapshot',
  ENABLE_PTZ_FOR_BUILTIN = 'enable-ptz-for-builtin',
  EXPERT = 'expert',
  PRINT_PERFORMANCE_LOGS = 'print-performance-logs',
  SAVE_METADATA = 'save-metadata',
  SHOW_ALL_RESOLUTIONS = 'show-all-resolutions',
  SHOW_METADATA = 'show-metadata',
}

/**
 * Enables or disables expert mode.
 *
 * @param enable Whether to enable or disable expert mode.
 */
export function setExpertMode(enable: boolean): void {
  state.set(ExpertOption.EXPERT, enable);
  localStorage.set(LocalStorageKey.EXPERT_MODE, enable);
}

/**
 * Toggles expert mode.
 */
export function toggleExpertMode(): void {
  // TODO(b/231535710): When toggle expert mode, also check the state of all
  // options under expert mode
  const newState = !state.get(ExpertOption.EXPERT);
  setExpertMode(newState);
}


/**
 * Get state value for expert mode and expert options.
 *
 * @param option Option state to be checked.
 */
export function isEnabled(option: ExpertOption): boolean {
  if (!state.get(ExpertOption.EXPERT)) {
    return false;
  }
  return state.get(option);
}

/**
 * Adds observer function to be called on expert options.
 *
 * @param option Option state to be observed.
 * @param observer Observer function called with newly changed value.
 */
export function addObserver(
    option: ExpertOption, observer: state.StateObserver): void {
  // Notify when isEnabled() value for option is changed
  state.addObserver(option, observer);
  state.addObserver(
      ExpertOption.EXPERT, (val: boolean, perfInfo: PerfInformation) => {
        // When Expert value changes, isEnabled() value will only change when
        // the option value is true, otherwise, isEnabled() is always false
        if (state.get(option)) {
          observer(val, perfInfo);
        }
      });
}
