// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export declare interface InterfaceA {
  someMethod?(text: string): Promise<void>;
  someRequiredMethod(text: string): Promise<void>;
  returnsEnum1(): ExtEnum1;
  takesEnum1(value: ExtEnum1): void;
  // ErrorAddRequiredMethod:edit-add-lines:
  // newRequired(): void;

  // OkAddOptionalMethod:edit-add-lines:
  // newOptional?(): void;
}

export declare interface InterfaceB {
  someMethod?(text: string): Promise<void>;
  someRequiredMethod(text: string): Promise<void>;
}

// ErrorInterfaceIsNotDeclare:edit-add-lines:
// export interface InterfaceC {}

export enum ExtEnum1 {
  A = 0,
  B = 1,
  // OkAddNewEnumValue:edit-add-lines:
  // C = 2,
}

export enum ExtEnum2 {
  A = 0,
  // ErrorEnumRemovedValue:edit-remove-lines: 1
  B = 1,
}

export enum ClosedEnum1 {
  A = 0,
  B = 1,
  // ErrorAddToClosedEnum:edit-add-lines:
  // C = 2,
}

export interface PrivateTypes {
  privateTypes: PrivateTypes;
  closedEnums: ClosedEnums;
}

export interface ClosedEnums {
  closedEnum1: typeof ClosedEnum1;
}
