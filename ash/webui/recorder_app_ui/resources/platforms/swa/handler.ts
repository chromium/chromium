// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * strings.m.js is generated when we enable it via UseStringsJs() in webUI
 * controller. When loading it, it will populate data such as localized strings
 * into |loadTimeData| that is imported below.
 */
import '../../strings.m.js';

import {
  ColorChangeUpdater,
} from
  'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {nothing} from 'chrome://resources/mwc/lit/index.js';

import {InternalMicInfo} from '../../core/microphone_manager.js';
import {
  Model,
  ModelId,
  ModelState,
} from '../../core/on_device_model/types.js';
import {
  PlatformHandler as PlatformHandlerBase,
} from '../../core/platform_handler.js';
import {Signal, signal} from '../../core/reactive/signal.js';
import {SodaSession} from '../../core/soda/types.js';
import {
  assertExhaustive,
  assertExists,
  assertNotReached,
} from '../../core/utils/assert.js';

import {OnDeviceModel} from './on_device_model.js';
import {MojoSodaSession} from './soda_session.js';
import {
  LoadModelResult,
  ModelState as MojoModelState,
  ModelStateMonitorReceiver,
  ModelStateType,
  OnDeviceModelRemote,
  PageHandler as MojoPageHandler,
  SodaClientReceiver,
  SodaRecognizerRemote,
} from './types.js';

function mojoModelStateToModelState(state: MojoModelState): ModelState {
  switch (state.type) {
    case ModelStateType.kNotInstalled:
      return {kind: 'notInstalled'};
    case ModelStateType.kInstalling:
      return {kind: 'installing', progress: assertExists(state.progress)};
    case ModelStateType.kInstalled:
      return {kind: 'installed'};
    case ModelStateType.kError:
      return {kind: 'error'};
    case ModelStateType.kUnavailable:
      return {kind: 'unavailable'};
    case ModelStateType.MIN_VALUE:
    case ModelStateType.MAX_VALUE:
      return assertNotReached(
        `Got MIN_VALUE or MAX_VALUE from mojo ModelStateType: ${state.type}`,
      );
    default:
      assertExhaustive(state.type);
  }
}

export class PlatformHandler extends PlatformHandlerBase {
  private readonly remote = MojoPageHandler.getRemote();

  readonly sodaState = signal<ModelState>({kind: 'unavailable'});

  readonly modelStates = new Map<ModelId, Signal<ModelState>>();

  constructor() {
    super();

    for (const modelId of Object.values(ModelId)) {
      this.modelStates.set(modelId, signal({kind: 'unavailable'}));
    }
  }

  override async init(): Promise<void> {
    ColorChangeUpdater.forDocument().start();

    const update = (state: MojoModelState) => {
      this.sodaState.value = mojoModelStateToModelState(state);
    };
    const monitor = new ModelStateMonitorReceiver({update});
    // This should be relatively quick since in recorder_app_ui.cc we just
    // return the cached state here, but we await here to avoid UI showing
    // temporary unavailabe state.
    const {state} = await this.remote.addSodaMonitor(
      monitor.$.bindNewPipeAndPassRemote(),
    );
    update(state);

    for (const modelId of this.modelStates.keys()) {
      const update = (state: MojoModelState) => {
        assertExists(this.modelStates.get(modelId)).value =
          mojoModelStateToModelState(state);
      };
      const monitor = new ModelStateMonitorReceiver({update});

      // This should be relatively quick since in recorder_app_ui.cc we just
      // return the cached state here, but we await here to avoid UI showing
      // temporary unavailabe state.
      const {state} = await this.remote.addModelMonitor(
        {value: modelId},
        monitor.$.bindNewPipeAndPassRemote(),
      );
      update(state);
    }
  }

  override async loadModel(modelId: ModelId): Promise<Model> {
    const newModel = new OnDeviceModelRemote();
    const {result} = await this.remote.loadModel(
      {value: modelId},
      newModel.$.bindNewPipeAndPassReceiver(),
    );
    if (result !== LoadModelResult.kSuccess) {
      // TODO(pihsun): Dedicated error type?
      throw new Error(`Load model failed: ${result}`);
    }
    return new OnDeviceModel(newModel);
  }

  override installSoda(): void {
    // We don't care about the returned promise as long as the request goes
    // through. The install progress is separately tracked in `sodaState`.
    void this.remote.installSoda();
  }

  override async newSodaSession(): Promise<SodaSession> {
    const recognizer = new SodaRecognizerRemote();
    const session = new MojoSodaSession(recognizer);
    const client = new SodaClientReceiver(session);
    const {result} = await this.remote.loadSpeechRecognizer(
      client.$.bindNewPipeAndPassRemote(),
      recognizer.$.bindNewPipeAndPassReceiver(),
    );
    if (!result) {
      // TODO(pihsun): Dedicated error type?
      throw new Error('Load soda failed');
    }
    return session;
  }

  override async getMicrophoneInfo(deviceId: string): Promise<InternalMicInfo> {
    const info = (await this.remote.getMicrophoneInfo(deviceId)).info;
    // TODO(kamchonlathorn): Consider if it should return the default value or
    // drop the mic from the list instead.
    return info ?? {isDefault: false, isInternal: false};
  }

  override getStringF(id: string, ...args: Array<number|string>): string {
    return loadTimeData.getStringF(id, ...args);
  }

  override renderDevUi(): RenderResult {
    return nothing;
  }

  override handleUncaughtError(_error: unknown): RenderResult|null {
    return null;
  }

  override showAiFeedbackDialog(description: string): void {
    this.remote.openAiFeedbackDialog(description);
  }

  override async getSystemAudioMediaStream(): Promise<MediaStream> {
    return navigator.mediaDevices.getDisplayMedia({
      // `video: false` can be used here with the special permission
      // DISPLAY_MEDIA_SYSTEM_AUDIO.
      video: false,
      audio: true,
      systemAudio: 'include',
    });
  }
}
