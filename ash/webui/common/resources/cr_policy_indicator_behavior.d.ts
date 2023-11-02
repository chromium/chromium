// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export enum CrPolicyIndicatorType {
  DEVICE_POLICY = 'devicePolicy',
  EXTENSION = 'extension',
  NONE = 'none',
  OWNER = 'owner',
  PRIMARY_USER = 'primary_user',
  RECOMMENDED = 'recommended',
  USER_POLICY = 'userPolicy',
  PARENT = 'parent',
  CHILD_RESTRICTION = 'childRestriction',
}

export interface CrPolicyIndicatorBehavior {
  indicatorType: CrPolicyIndicatorType;
  indicatorSourceName: string;
  indicatorVisible: boolean;
  indicatorIcon: string;
  getIndicatorTooltip(
      type: CrPolicyIndicatorType, name: string, matches?: boolean): string;
}

declare const CrPolicyIndicatorBehavior: object;
