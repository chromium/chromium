// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExperimentalTriggeringClientInterface, ExperimentalTriggeringUpdatesHandlerRemote, Screenshot as ScreenshotMojo} from '../../glic.mojom-webui.js';
import {screenshotToClient} from '../host/conversions.js';
import type {GlicApiHost} from '../host/glic_api_host.js';
import {ResponseExtras} from '../transport/messaging.js';
import type {PostMessageRemote} from '../transport/post_message_transport.js';

import type {ExperimentalTriggeringClient} from './experimental_triggering_types.js';

export class ExperimentalTriggeringClientImpl implements
    ExperimentalTriggeringClientInterface {
  constructor(
      private sender: PostMessageRemote<ExperimentalTriggeringClient>,
      private host: GlicApiHost) {}

  async getExperimentalTriggeringUpdates(
      handler: ExperimentalTriggeringUpdatesHandlerRemote):
      Promise<{success: boolean}> {
    const id = this.host.addExperimentalTriggeringUpdatesHandler(handler);
    try {
      const result = await this.sender.requestWithResponse(
          'getExperimentalTriggeringUpdates', {
            observationId: id,
          });
      if (!result.success) {
        this.host.deleteExperimentalTriggeringUpdatesHandler(id);
      }
      return {success: result.success};
    } catch (e) {
      this.host.deleteExperimentalTriggeringUpdatesHandler(id);
      throw e;
    }
  }

  async uploadEncryptedScreenshot(screenshot: ScreenshotMojo):
      Promise<{fileToken: string | null}> {
    const extras = new ResponseExtras();
    const clientScreenshot = screenshotToClient(screenshot, extras);
    if (!clientScreenshot) {
      return {fileToken: null};
    }
    try {
      const result = await this.sender.requestWithResponse(
          'uploadEncryptedScreenshot', {
            screenshot: clientScreenshot,
          },
          extras.transfers);
      return {fileToken: result.fileToken};
    } catch (e) {
      console.warn(
          'Failed to upload encrypted screenshot over postMessage:', e);
      return {fileToken: null};
    }
  }
}
