// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {Value} from '//resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

import {UntrustedProjectorPageCallbackRouter, UntrustedProjectorPageHandlerFactory, UntrustedProjectorPageHandlerRemote, UntrustedProjectorPageRemote} from './ash/webui/projector_app/mojom/untrusted_projector.mojom-webui.js';
import {PrefsThatProjectorCanAskFor} from './ash/webui/projector_app/public/mojom/projector_types.mojom-webui.js';

const booleanUserPrefs = new Map([
  [
    'ash.projector.creation_flow_enabled',
    PrefsThatProjectorCanAskFor.kProjectorCreationFlowEnabled,
  ],
  [
    'ash.projector.exclude_transcript_dialog_shown',
    PrefsThatProjectorCanAskFor.kProjectorExcludeTranscriptDialogShown,
  ],
]);

const intUserPrefs = new Map([
  [
    'ash.projector.gallery_onboarding_show_count',
    PrefsThatProjectorCanAskFor.kProjectorGalleryOnboardingShowCount,
  ],
  [
    'ash.projector.viewer_onboarding_show_count',
    PrefsThatProjectorCanAskFor.kProjectorViewerOnboardingShowCount,
  ],
]);

export class UntrustedProjectorBrowserProxyImpl {
  constructor() {
    this.pageHandlerFactory = UntrustedProjectorPageHandlerFactory.getRemote();
    this.pageHandlerRemote = new UntrustedProjectorPageHandlerRemote();
    this.projectorCallbackRouter = new UntrustedProjectorPageCallbackRouter();
    this.pageHandlerFactory.create(
        this.pageHandlerRemote.$.bindNewPipeAndPassReceiver(),
        this.projectorCallbackRouter.$.bindNewPipeAndPassRemote());
  }

  getProjectorCallbackRouter() {
    return this.projectorCallbackRouter;
  }

  async getNewScreencastPreconditionState() {
    const {precondition} =
        await this.pageHandlerRemote.getNewScreencastPrecondition();
    return precondition;
  }

  async shouldDownloadSoda() {
    const {shouldDownload} = await this.pageHandlerRemote.shouldDownloadSoda();
    return shouldDownload;
  }

  async installSoda() {
    const {triggered} = await this.pageHandlerRemote.installSoda();
    return triggered;
  }

  async getPendingScreencasts() {
    const {pendingScreencasts} =
        await this.pageHandlerRemote.getPendingScreencasts();
    return pendingScreencasts;
  }

  async getUserPref(userPref) {
    const isBoolPref = booleanUserPrefs.has(userPref);
    const isIntPref = intUserPrefs.has(userPref);

    if (!(isBoolPref || isIntPref)) {
      throw new Error(`Unsupported user preference: ${userPref}`);
    }

    let mojoPref;
    if (isBoolPref) {
      mojoPref = booleanUserPrefs.get(userPref);
    } else {
      mojoPref = intUserPrefs.get(userPref);
    }

    const {value} = await this.pageHandlerRemote.getUserPref(mojoPref);

    if (isBoolPref && value.hasOwnProperty('boolValue')) {
      return value.boolValue;
    }

    if (isIntPref && value.hasOwnProperty('intValue')) {
      return value.intValue;
    }

    throw new Error(
        `Returned pref value with unexpected type for user preference: ${
            userPref}`);
  }

  async setUserPref(userPref, value) {
    const isBoolPref = booleanUserPrefs.has(userPref);
    const isIntPref = intUserPrefs.has(userPref);

    if (!(isBoolPref || isIntPref)) {
      throw new Error(`Unsupported user preference: ${userPref}`);
    }

    let mojoPref;
    const mojoValue = new Object();
    if (isBoolPref) {
      mojoPref = booleanUserPrefs.get(userPref);
      mojoValue.boolValue = value;
    } else {
      mojoPref = intUserPrefs.get(userPref);
      mojoValue.intValue = value;
    }

    await this.pageHandlerRemote.setUserPref(mojoPref, mojoValue);
    return true;
  }

  async openFeedbackDialog() {
    await this.pageHandlerRemote.openFeedbackDialog();
    return;
  }
}

/**
 * @type {UntrustedProjectorBrowserProxyImpl}
 */
export const browserProxy = new UntrustedProjectorBrowserProxyImpl();
