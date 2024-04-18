// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export enum VoicePackStatus {
  NOT_INSTALLED,
  INSTALLING,
  INSTALLED,
  ERROR,
}

// The following possible values of "status" is a union of enum values of
// enum InstallationState and enum ErrorCode in read_anything.mojom
export function mojoVoicePackStatusToVoicePackStatusEnum(
    mojoPackStatus: string) {
  if (mojoPackStatus === 'kNotInstalled') {
    return VoicePackStatus.NOT_INSTALLED;
  } else if (mojoPackStatus === 'kInstalling') {
    return VoicePackStatus.INSTALLING;
  } else if (mojoPackStatus === 'kInstalled') {
    return VoicePackStatus.INSTALLED;
  }
  // The success statuses were not sent so return an Error
  // TODO (b/331795122) Handle install errors on the UI
  return VoicePackStatus.ERROR;
}
