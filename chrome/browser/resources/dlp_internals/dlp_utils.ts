// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebContentsInfo} from './dlp_internals.mojom-webui.js';
import {ContentRestriction, DlpEvent_Mode, DlpEvent_Restriction, DlpEvent_UserType, EndpointType, EventDestination_Component, Level} from './dlp_internals.mojom-webui.js';

export const EndpointTypeMap = {
  [EndpointType.kDefault]: 'Default',
  [EndpointType.kUrl]: 'URL',
  [EndpointType.kClipboardHistory]: 'Clipboard History',
  [EndpointType.kUnknownVm]: 'Unknown VM',
  [EndpointType.kArc]: 'Arc',
  [EndpointType.kBorealis]: 'Borealis',
  [EndpointType.kCrostini]: 'Crostini',
  [EndpointType.kPluginVm]: 'Plugin VM',
  [EndpointType.kLacros]: 'Lacros',
};

export const DestinationComponentMap = {
  [EventDestination_Component.kUndefinedComponent]: 'Undefined Component',
  [EventDestination_Component.kArc]: 'ARC',
  [EventDestination_Component.kCrostini]: 'Crostini',
  [EventDestination_Component.kPluginVm]: 'PluginVm',
  [EventDestination_Component.kUsb]: 'USB',
  [EventDestination_Component.kDrive]: 'GoogleDrive',
  [EventDestination_Component.kOnedrive]: 'OneDrive',
};

export const EventRestrictionMap = {
  [DlpEvent_Restriction.kUndefinedRestriction]: 'Undefined Restriction',
  [DlpEvent_Restriction.kClipboard]: 'Clipboard',
  [DlpEvent_Restriction.kScreenshot]: 'Screenshot',
  [DlpEvent_Restriction.kScreencast]: 'Screencast',
  [DlpEvent_Restriction.kPrinting]: 'Printing',
  [DlpEvent_Restriction.kEprivacy]: 'ePrivacy',
  [DlpEvent_Restriction.kFiles]: 'Files',
};

export const EventModeMap = {
  [DlpEvent_Mode.kUndefinedMode]: 'Undefined Mode',
  [DlpEvent_Mode.kBlock]: 'Block',
  [DlpEvent_Mode.kReport]: 'Report',
  [DlpEvent_Mode.kWarn]: 'Warning',
  [DlpEvent_Mode.kWarnProceed]: 'Warning Proceeded',
};

export const EventUserTypeMap = {
  [DlpEvent_UserType.kUndefinedUserType]: 'Undefined User',
  [DlpEvent_UserType.kRegular]: 'Regular',
  [DlpEvent_UserType.kManagedGuest]: 'Managed Guest',
  [DlpEvent_UserType.kKiosk]: 'Kiosk',
};

export const ContentRestrictionMap = {
  [ContentRestriction.kScreenshot]: 'Screenshot',
  [ContentRestriction.kPrivacyScreen]: 'Privacy Screen',
  [ContentRestriction.kPrint]: 'Printing',
  [ContentRestriction.kScreenShare]: 'Screenshare',
};

export const LevelMap = {
  [Level.kNotSet]: 'NOT SET',
  [Level.kReport]: 'REPORT',
  [Level.kWarn]: 'WARN',
  [Level.kBlock]: 'BLOCK',
  [Level.kAllow]: 'ALLOW',
};

export class WebContentsElement {
  private info_: WebContentsInfo;
  private framesExpanded_: boolean = false;

  constructor(webContentsInfo: WebContentsInfo) {
    this.info_ = webContentsInfo;
    this.framesExpanded_ = false;
  }
}
