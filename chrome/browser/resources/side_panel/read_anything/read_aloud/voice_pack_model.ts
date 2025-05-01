// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VoiceClientSideStatusCode, VoicePackStatus} from '../voice_language_util.js';

export class VoicePackModel {
  // Local representation of the status of voice pack downloads and
  // availability.
  private voiceStatusLocalState_: Map<string, VoiceClientSideStatusCode> =
      new Map();

  // Cache of responses from LanguagePackManager.
  private voicePackInstallStatusServerResponses_: Map<string, VoicePackStatus> =
      new Map();

  getServerStatus(lang: string): VoicePackStatus|null {
    const status = this.voicePackInstallStatusServerResponses_.get(lang);
    return (status === undefined) ? null : status;
  }

  setServerStatus(lang: string, status: VoicePackStatus) {
    this.voicePackInstallStatusServerResponses_.set(lang, status);
  }

  getLocalStatus(lang: string): VoiceClientSideStatusCode|null {
    const status = this.voiceStatusLocalState_.get(lang);
    return (status === undefined) ? null : status;
  }

  setLocalStatus(lang: string, status: VoiceClientSideStatusCode) {
    this.voiceStatusLocalState_.set(lang, status);
  }

  getServerLanguages(): string[] {
    return Array.from(this.voicePackInstallStatusServerResponses_.keys());
  }
}
