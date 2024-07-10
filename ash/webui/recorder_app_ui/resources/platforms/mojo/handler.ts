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

import {
  Model,
  PlatformHandler as PlatformHandlerBase,
  SodaSession,
  SodaState,
} from '../../core/platform_handler.js';
import {signal} from '../../core/reactive/signal.js';
import {
  assertExhaustive,
  assertExists,
  assertNotReached,
} from '../../core/utils/assert.js';

import {OnDeviceModel} from './on_device_model.js';
import {MojoSodaSession} from './soda_session.js';
import {
  LoadModelResult,
  OnDeviceModelRemote,
  PageHandler as MojoPageHandler,
  SodaClientReceiver,
  SodaRecognizerRemote,
  SodaState as MojoSodaState,
  SodaStateMonitorReceiver,
  SodaStateType,
} from './types.js';

function mojoSodaStateToSodaState(state: MojoSodaState): SodaState {
  switch (state.type) {
    case SodaStateType.kNotInstalled:
      return {kind: 'notInstalled'};
    case SodaStateType.kInstalling:
      return {kind: 'installing', progress: assertExists(state.progress)};
    case SodaStateType.kInstalled:
      return {kind: 'installed'};
    case SodaStateType.kError:
      return {kind: 'error'};
    case SodaStateType.kUnavailable:
      return {kind: 'unavailable'};
    case SodaStateType.MIN_VALUE:
    case SodaStateType.MAX_VALUE:
      return assertNotReached(
        `Got MIN_VALUE or MAX_VALUE from mojo SodaStateType: ${state.type}`,
      );
    default:
      assertExhaustive(state.type);
  }
}

export class PlatformHandler extends PlatformHandlerBase {
  private readonly remote = MojoPageHandler.getRemote();

  readonly sodaState = signal<SodaState>({kind: 'unavailable'});

  override async init(): Promise<void> {
    ColorChangeUpdater.forDocument().start();

    const monitor = new SodaStateMonitorReceiver({
      update: (state) => {
        this.sodaState.value = mojoSodaStateToSodaState(state);
      },
    });
    // This should be relatively quick since in recorder_app_ui.cc we just
    // return the cached state here.
    // TODO(pihsun): Only do this after the first `getSodaState` if performance
    // is an issue.
    const {state} = await this.remote.addSodaMonitor(
      monitor.$.bindNewPipeAndPassRemote(),
    );
    this.sodaState.value = mojoSodaStateToSodaState(state);
  }

  override async loadModel(uuid: string): Promise<Model> {
    const newModel = new OnDeviceModelRemote();
    const {result} = await this.remote.loadModel(
      {value: uuid},
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

  override getStringF(id: string, ...args: Array<number|string>): string {
    return loadTimeData.getStringF(id, ...args);
  }

  override renderDevUi(): RenderResult {
    return nothing;
  }

  override handleUncaughtError(_error: unknown): RenderResult|null {
    return null;
  }
}
