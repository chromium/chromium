// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/public/mojom/projector_types_mojo_traits.h"

#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "base/notreached.h"

namespace mojo {

MojomNewScreencastPreconditionState
EnumTraits<MojomNewScreencastPreconditionState,
           ash::NewScreencastPreconditionState>::
    ToMojom(ash::NewScreencastPreconditionState input) {
  switch (input) {
    case ash::NewScreencastPreconditionState::kDisabled:
      return MojomNewScreencastPreconditionState::kDisabled;
    case ash::NewScreencastPreconditionState::kEnabled:
      return MojomNewScreencastPreconditionState::kEnabled;
    case ash::NewScreencastPreconditionState::kHidden:
      return MojomNewScreencastPreconditionState::kHidden;
  }
  NOTREACHED();
}

bool EnumTraits<MojomNewScreencastPreconditionState,
                ash::NewScreencastPreconditionState>::
    FromMojom(MojomNewScreencastPreconditionState input,
              ash::NewScreencastPreconditionState* out) {
  switch (input) {
    case MojomNewScreencastPreconditionState::kEnabled:
      *out = ash::NewScreencastPreconditionState::kEnabled;
      return true;
    case MojomNewScreencastPreconditionState::kDisabled:
      *out = ash::NewScreencastPreconditionState::kDisabled;
      return true;
    case MojomNewScreencastPreconditionState::kHidden:
      *out = ash::NewScreencastPreconditionState::kHidden;
      return true;
  }
  return false;
}

MojomNewScreencastPreconditionReason
EnumTraits<MojomNewScreencastPreconditionReason,
           ash::NewScreencastPreconditionReason>::
    ToMojom(ash::NewScreencastPreconditionReason input) {
  switch (input) {
    case ash::NewScreencastPreconditionReason::
        kOnDeviceSpeechRecognitionNotSupported:
      return MojomNewScreencastPreconditionReason::
          kOnDeviceRecognitionNotSupported;
    case ash::NewScreencastPreconditionReason::kUserLocaleNotSupported:
      return MojomNewScreencastPreconditionReason::kUserLocaleNotSupported;
    case ash::NewScreencastPreconditionReason::kInProjectorSession:
      return MojomNewScreencastPreconditionReason::kInProjectorSession;
    case ash::NewScreencastPreconditionReason::kScreenRecordingInProgress:
      return MojomNewScreencastPreconditionReason::kScreenRecordingInProgress;
    case ash::NewScreencastPreconditionReason::kSodaDownloadInProgress:
      return MojomNewScreencastPreconditionReason::kSodaDownloadInProgress;
    case ash::NewScreencastPreconditionReason::kOutOfDiskSpace:
      return MojomNewScreencastPreconditionReason::kOutOfDiskSpace;
    case ash::NewScreencastPreconditionReason::kNoMic:
      return MojomNewScreencastPreconditionReason::kNoMic;
    case ash::NewScreencastPreconditionReason::kDriveFsUnmounted:
      return MojomNewScreencastPreconditionReason::kDriveFSUnMounted;
    case ash::NewScreencastPreconditionReason::kDriveFsMountFailed:
      return MojomNewScreencastPreconditionReason::kDriveFSMountFailed;
    case ash::NewScreencastPreconditionReason::kOthers:
      return MojomNewScreencastPreconditionReason::kOthers;
    case ash::NewScreencastPreconditionReason::
        kSodaInstallationErrorUnspecified:
      return MojomNewScreencastPreconditionReason::
          kSodaInstallationErrorUnspecifiedError;
    case ash::NewScreencastPreconditionReason::
        kSodaInstallationErrorNeedsReboot:
      return MojomNewScreencastPreconditionReason::
          kSodaInstallationErrorNeedsReboot;
    case ash::NewScreencastPreconditionReason::kAudioCaptureDisabledByPolicy:
      return MojomNewScreencastPreconditionReason::
          kAudioCaptureDisabledByPolicy;
    case ash::NewScreencastPreconditionReason::kEnabledBySoda:
      return MojomNewScreencastPreconditionReason::kEnabledBySoda;
    case ash::NewScreencastPreconditionReason::
        kEnabledByServerSideSpeechRecognition:
      return MojomNewScreencastPreconditionReason::
          kEnabledByServerSideSpeechRecognition;
  }

  NOTREACHED();
}

bool EnumTraits<MojomNewScreencastPreconditionReason,
                ash::NewScreencastPreconditionReason>::
    FromMojom(MojomNewScreencastPreconditionReason input,
              ash::NewScreencastPreconditionReason* out) {
  switch (input) {
    case MojomNewScreencastPreconditionReason::
        kSodaInstallationErrorUnspecifiedError:
      *out = ash::NewScreencastPreconditionReason::
          kSodaInstallationErrorUnspecified;
      return true;
    case MojomNewScreencastPreconditionReason::kOnDeviceRecognitionNotSupported:
      *out = ash::NewScreencastPreconditionReason::
          kOnDeviceSpeechRecognitionNotSupported;
      return true;
    case MojomNewScreencastPreconditionReason::kUserLocaleNotSupported:
      *out = ash::NewScreencastPreconditionReason::kUserLocaleNotSupported;
      return true;
    case MojomNewScreencastPreconditionReason::kInProjectorSession:
      *out = ash::NewScreencastPreconditionReason::kInProjectorSession;
      return true;
    case MojomNewScreencastPreconditionReason::kScreenRecordingInProgress:
      *out = ash::NewScreencastPreconditionReason::kScreenRecordingInProgress;
      return true;
    case MojomNewScreencastPreconditionReason::kSodaDownloadInProgress:
      *out = ash::NewScreencastPreconditionReason::kSodaDownloadInProgress;
      return true;
    case MojomNewScreencastPreconditionReason::kOutOfDiskSpace:
      *out = ash::NewScreencastPreconditionReason::kOutOfDiskSpace;
      return true;
    case MojomNewScreencastPreconditionReason::kNoMic:
      *out = ash::NewScreencastPreconditionReason::kNoMic;
      return true;
    case MojomNewScreencastPreconditionReason::kDriveFSUnMounted:
      *out = ash::NewScreencastPreconditionReason::kDriveFsUnmounted;
      return true;
    case MojomNewScreencastPreconditionReason::kDriveFSMountFailed:
      *out = ash::NewScreencastPreconditionReason::kDriveFsMountFailed;
      return true;
    case MojomNewScreencastPreconditionReason::kOthers:
      *out = ash::NewScreencastPreconditionReason::kOthers;
      return true;
    case MojomNewScreencastPreconditionReason::
        kSodaInstallationErrorNeedsReboot:
      *out = ash::NewScreencastPreconditionReason::
          kSodaInstallationErrorNeedsReboot;
      return true;
    case MojomNewScreencastPreconditionReason::kAudioCaptureDisabledByPolicy:
      *out =
          ash::NewScreencastPreconditionReason::kAudioCaptureDisabledByPolicy;
      return true;
    case MojomNewScreencastPreconditionReason::kEnabledBySoda:
      *out = ash::NewScreencastPreconditionReason::kEnabledBySoda;
      return true;
    case MojomNewScreencastPreconditionReason::
        kEnabledByServerSideSpeechRecognition:
      *out = ash::NewScreencastPreconditionReason::
          kEnabledByServerSideSpeechRecognition;
      return true;
  }

  return false;
}

ash::NewScreencastPreconditionState StructTraits<
    MojomNewScreencastPreconditioDataView,
    ash::NewScreencastPrecondition>::state(const ash::NewScreencastPrecondition&
                                               r) {
  return r.state;
}

std::vector<ash::NewScreencastPreconditionReason>
StructTraits<MojomNewScreencastPreconditioDataView,
             ash::NewScreencastPrecondition>::
    reasons(const ash::NewScreencastPrecondition& r) {
  return r.reasons;
}

bool StructTraits<MojomNewScreencastPreconditioDataView,
                  ash::NewScreencastPrecondition>::
    Read(MojomNewScreencastPreconditioDataView data,
         ash::NewScreencastPrecondition* out) {
  return data.ReadState(&out->state) && data.ReadReasons(&out->reasons);
}

}  // namespace mojo
