// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains many long export lines that exceed the max-len limit
/* eslint-disable @stylistic/max-len */

export type {
  BigBuffer,
} from
    'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
export type {
  PointF,
} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
export {
  CameraIntentAction,
} from '../../mojom/ash/components/arc/mojom/camera_intent.mojom-webui.js';
export {
  CameraAppHelper,
  CameraAppHelperRemote,
  CameraUsageOwnershipMonitorCallbackRouter,
  ExternalScreenMonitorCallbackRouter,
  FileMonitorResult,
  LidState,
  LidStateMonitorCallbackRouter,
  ScreenLockedMonitorCallbackRouter,
  ScreenState,
  ScreenStateMonitorCallbackRouter,
  StorageMonitorCallbackRouter,
  StorageMonitorStatus,
  SWPrivacySwitchMonitorCallbackRouter,
  TabletModeMonitorCallbackRouter,
  WindowStateControllerRemote,
  WindowStateMonitorCallbackRouter,
  WindowStateType,
} from '../../mojom/ash/webui/camera_app_ui/camera_app_helper.mojom-webui.js';
export type {
  WifiConfig,
} from '../../mojom/ash/webui/camera_app_ui/camera_app_helper.mojom-webui.js';
export {
  AndroidIntentResultType,
  AspectRatioSet,
  BarcodeContentType,
  DocScanActionType,
  DocScanFixType,
  DocScanResultType,
  Facing,
  GifResultType,
  GridType,
  LaunchType,
  LowStorageActionType,
  Mode,
  OcrEventType,
  PerfEventType,
  Pressure,
  RecordType,
  ResolutionLevel,
  ShutterType,
  TimerType,
  UserBehavior,
} from '../../mojom/ash/webui/camera_app_ui/events_sender.mojom-webui.js';
export type {
  CaptureEventParams,
  EventsSenderRemote,
} from '../../mojom/ash/webui/camera_app_ui/events_sender.mojom-webui.js';
export type {
  Line as OcrResultLine,
  OcrResult,
} from '../../mojom/ash/webui/camera_app_ui/ocr.mojom-webui.js';
export {
  PdfBuilderRemote,
} from '../../mojom/ash/webui/camera_app_ui/pdf_builder.mojom-webui.js';
export {
  WifiEapMethod,
  WifiEapPhase2Method,
  WifiSecurityType,
} from '../../mojom/ash/webui/camera_app_ui/types.mojom-webui.js';
export {
  Rotation,
} from
    '../../mojom/chromeos/services/machine_learning/public/mojom/document_scanner_param_types.mojom-webui.js';
export type {
  Blob as MojoBlob,
} from '../../mojom/media/capture/mojom/image_capture.mojom-webui.js';
export {
  CameraAppDeviceProvider,
  CameraAppDeviceProviderRemote,
  CameraAppDeviceRemote,
  CameraEventObserverCallbackRouter,
  CameraInfoObserverCallbackRouter,
  CaptureIntent,
  DocumentCornersObserverCallbackRouter,
  Effect,
  GetCameraAppDeviceStatus,
  ResultMetadataObserverCallbackRouter,
  StillCaptureResultObserverCallbackRouter,
  StreamType,
} from
    '../../mojom/media/capture/video/chromeos/mojom/camera_app.mojom-webui.js';
export {
  CameraFacing,
} from
    '../../mojom/media/capture/video/chromeos/mojom/camera_common.mojom-webui.js';
export type {
  CameraInfo,
} from
    '../../mojom/media/capture/video/chromeos/mojom/camera_common.mojom-webui.js';
export {
  PortraitModeSegResult,
} from
    '../../mojom/media/capture/video/chromeos/mojom/camera_features.mojom-webui.js';
export {
  EntryType,
} from
    '../../mojom/media/capture/video/chromeos/mojom/camera_metadata.mojom-webui.js';
export type {
  CameraMetadata,
  CameraMetadataEntry,
} from
    '../../mojom/media/capture/video/chromeos/mojom/camera_metadata.mojom-webui.js';
export {
  AndroidControlAeAntibandingMode,
  AndroidControlAeMode,
  AndroidControlAeState,
  AndroidControlAfMode,
  AndroidControlAfState,
  AndroidControlAwbMode,
  AndroidControlAwbState,
  AndroidInfoSupportedHardwareLevel,
  AndroidStatisticsFaceDetectMode,
  CameraMetadataTag,
} from
    '../../mojom/media/capture/video/chromeos/mojom/camera_metadata_tags.mojom-webui.js';
