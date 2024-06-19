// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  ColorChangeUpdater,
} from
    'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {nothing} from 'chrome://resources/mwc/lit/index.js';

import {
  Model,
  PlatformHandler as PlatformHandlerBase,
  SodaSession,
} from '../../core/platform_handler.js';
import {SodaClientReceiver} from '../../mojom/soda.mojom-webui.js';

import {OnDeviceModel} from './on_device_model.js';
import {MojoSodaSession} from './soda_session.js';
import {
  LoadModelResult,
  OnDeviceModelRemote,
  PageHandler as MojoPageHandler,
  SodaRecognizerRemote,
} from './types.js';

export class PlatformHandler extends PlatformHandlerBase {
  private readonly remote = MojoPageHandler.getRemote();

  override init(): void {
    ColorChangeUpdater.forDocument().start();
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

  override i18n(name: string, ..._substitutions: Array<number|string>): string {
    // TODO(pihsun): Call load_time_data.
    return name;
  }

  override renderDevUi(): RenderResult {
    return nothing;
  }
}
