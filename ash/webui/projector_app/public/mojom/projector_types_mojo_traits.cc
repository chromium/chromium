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

ash::NewScreencastPreconditionState
EnumTraits<MojomNewScreencastPreconditionState,
           ash::NewScreencastPreconditionState>::
    FromMojom(MojomNewScreencastPreconditionState input) {
  switch (input) {
    case MojomNewScreencastPreconditionState::kEnabled:
      return ash::NewScreencastPreconditionState::kEnabled;
    case MojomNewScreencastPreconditionState::kDisabled:
      return ash::NewScreencastPreconditionState::kDisabled;
    case MojomNewScreencastPreconditionState::kHidden:
      return ash::NewScreencastPreconditionState::kHidden;
  }
  NOTREACHED();
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

ash::NewScreencastPreconditionReason
EnumTraits<MojomNewScreencastPreconditionReason,
           ash::NewScreencastPreconditionReason>::
    FromMojom(MojomNewScreencastPreconditionReason input) {
  switch (input) {
    case MojomNewScreencastPreconditionReason::
        kSodaInstallationErrorUnspecifiedError:
      return ash::NewScreencastPreconditionReason::
          kSodaInstallationErrorUnspecified;
    case MojomNewScreencastPreconditionReason::kOnDeviceRecognitionNotSupported:
      return ash::NewScreencastPreconditionReason::
          kOnDeviceSpeechRecognitionNotSupported;
    case MojomNewScreencastPreconditionReason::kUserLocaleNotSupported:
      return ash::NewScreencastPreconditionReason::kUserLocaleNotSupported;
    case MojomNewScreencastPreconditionReason::kInProjectorSession:
      return ash::NewScreencastPreconditionReason::kInProjectorSession;
    case MojomNewScreencastPreconditionReason::kScreenRecordingInProgress:
      return ash::NewScreencastPreconditionReason::kScreenRecordingInProgress;
    case MojomNewScreencastPreconditionReason::kSodaDownloadInProgress:
      return ash::NewScreencastPreconditionReason::kSodaDownloadInProgress;
    case MojomNewScreencastPreconditionReason::kOutOfDiskSpace:
      return ash::NewScreencastPreconditionReason::kOutOfDiskSpace;
    case MojomNewScreencastPreconditionReason::kNoMic:
      return ash::NewScreencastPreconditionReason::kNoMic;
    case MojomNewScreencastPreconditionReason::kDriveFSUnMounted:
      return ash::NewScreencastPreconditionReason::kDriveFsUnmounted;
    case MojomNewScreencastPreconditionReason::kDriveFSMountFailed:
      return ash::NewScreencastPreconditionReason::kDriveFsMountFailed;
    case MojomNewScreencastPreconditionReason::kOthers:
      return ash::NewScreencastPreconditionReason::kOthers;
    case MojomNewScreencastPreconditionReason::
        kSodaInstallationErrorNeedsReboot:
      return ash::NewScreencastPreconditionReason::
          kSodaInstallationErrorNeedsReboot;
    case MojomNewScreencastPreconditionReason::kAudioCaptureDisabledByPolicy:
      return ash::NewScreencastPreconditionReason::
          kAudioCaptureDisabledByPolicy;
    case MojomNewScreencastPreconditionReason::kEnabledBySoda:
      return ash::NewScreencastPreconditionReason::kEnabledBySoda;
    case MojomNewScreencastPreconditionReason::
        kEnabledByServerSideSpeechRecognition:
      return ash::NewScreencastPreconditionReason::
          kEnabledByServerSideSpeechRecognition;
  }

  NOTREACHED();
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
