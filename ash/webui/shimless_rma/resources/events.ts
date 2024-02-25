// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RmadErrorCode, StateResult} from './shimless_rma.mojom-webui.js';

export const DISABLE_NEXT_BUTTON = 'disable-next-button';
export type DisableNextButtonEvent = CustomEvent<boolean>;

export const DISABLE_ALL_BUTTONS = 'disable-all-buttons';
export type DisableAllButtonsEvent =
    CustomEvent<{showBusyStateOverlay: boolean}>;

export const ENABLE_ALL_BUTTONS = 'enable-all-buttons';
export type EnableAllButtonsEvent = CustomEvent;

export const TRANSITION_STATE = 'transition-state';
export type TransitionStateEvent = CustomEvent<
    () => Promise<{stateResult: StateResult, error?: RmadErrorCode}>>;

export const CLICK_NEXT_BUTTON = 'click-next-button';
export type ClickNextButtonEvent = CustomEvent;

export const CLICK_EXIT_BUTTON = 'click-exit-button';
export type ClickExitButtonEvent = CustomEvent;

export const SET_NEXT_BUTTON_LABEL = 'set-next-button-label';
export type SetNextButtonLabelEvent = CustomEvent<string>;

export const CLICK_REPAIR_COMPONENT_BUTTON = 'click-repair-component-button';
export type ClickRepairComponentButtonEvent = CustomEvent<number>;

export type OnSelectedChangedEvent = CustomEvent<{value: string}>;

export const CLICK_CALIBRATION_COMPONENT_BUTTON =
    'click-calibration-component-button';
export type ClickCalibrationComponentEvent = CustomEvent<number>;

export const FATAL_HARDWARE_ERROR = 'fatal-hardware-error';
export type FatalHardwareEvent = CustomEvent<{
  rmadErrorCode: RmadErrorCode,
  fatalErrorCode: number,
}>;

export const OPEN_LOGS_DIALOG = 'open-logs-dialog';
export type OpenLogsDialogEvent = CustomEvent;

type ExtractDetail<T> = T extends CustomEvent<infer U>? U : never;

/**
 * Constructs a CustomEvent with the given event type and details.
 * The event will bubble up through elements and components.
 */
export function createCustomEvent<T extends keyof HTMLElementEventMap>(
    type: T,
    detail: ExtractDetail<HTMLElementEventMap[T]>): CustomEvent<typeof detail> {
  return new CustomEvent(type, {bubbles: true, composed: true, detail});
}
