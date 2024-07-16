// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file re-export all used mojo types from generated
 * .mojom-webui.ts files. This makes it easier to mock all the imports on dev /
 * bundle.
 */

export {
  LoadModelResult,
  OnDeviceModelRemote,
  type ResponseChunk,
  type ResponseSummary,
  SessionRemote,
  StreamingResponderCallbackRouter,
} from '../../mojom/on_device_model.mojom-webui.js';
export {
  type ModelState,
  ModelStateMonitorReceiver,
  ModelStateType,
  PageHandler,
} from '../../mojom/recorder_app.mojom-webui.js';
export {
  type SodaClientInterface,
  SodaClientReceiver,
  SodaRecognizerRemote,
} from '../../mojom/soda.mojom-webui.js';
