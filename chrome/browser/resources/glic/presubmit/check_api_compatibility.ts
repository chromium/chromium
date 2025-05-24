// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See ../PRESUBMIT.py.
import type * as old from '@tmp/old_glic_api.js';

import type * as current from '../glic_api/glic_api.js';

// Warning! The checks in this file are not a complete guarantee of API
// compatibility.

/* eslint-disable-next-line @typescript-eslint/naming-convention */
function assertNever<_T extends never>() {}
type AllValues<T> = T[keyof T];

// TODO: This doesn't recurse into function parameter or return types, which
// means we're missing compatibility checks for parameter/return objects
// declared inline.
type DeepRequired<T> = {
  [K in keyof T]: DeepRequired<T[K]>
}&Required<T>;

// Get the set of BackwardsCompatibleTypes in both old and current. This
// allows us to ignore types removed from BackwardsCompatibleTypes.
type OldTypes = {
  [K in keyof old.BackwardsCompatibleTypes &
   keyof current.BackwardsCompatibleTypes]: old.BackwardsCompatibleTypes[K]
};
type CurrentTypes = {
  [K in keyof old.BackwardsCompatibleTypes &
   keyof current.BackwardsCompatibleTypes]: current.BackwardsCompatibleTypes[K]
};

/*
These are the kinds of changes we might see, and how they're categorized.

* OK:
  * Adding an optional field. {x:number} --> {x:number; y?: string}
  * Adding an optional parameter. foo():void -> foo(x?:number)

* ERROR:
  * Removing any field.       {x?:number} --> {}
  * Adding a required field.  {} --> {x:number}
  * Adding a required parameter. foo():void -> foo(x:number)
  * Widening a field type.    {x:number} --> {x:number|string}
     (Changing a host type in this way is likely not compatible for old versions
      of Chrome.)
*/

// Note: We're just using assignment to verify these types are compatible.

export const oldTypesAreCompatibleWithCurrent: CurrentTypes =
    null as any as old.BackwardsCompatibleTypes;
export const currentTypesAreCompatibleWithOld: OldTypes =
    null as any as current.BackwardsCompatibleTypes;

// Make all fields required, then check that all fields are compatible. This
// ensures we don't remove optional fields.
export const canNotRemoveAnything: DeepRequired<OldTypes> =
    null as any as DeepRequired<current.BackwardsCompatibleTypes>;

// Ensure ClosedEnums are not modified, and ExtensibleEnums are only extended.
// TODO: This only checks enum keys. Not sure how to check values.
type EnumOnlyExtended<O, N> = Exclude<keyof O, keyof N> extends never ?
    never :
    ['Error: enum changed', O];
type EnumIsEquivalent<O, N> = Exclude<keyof N, keyof O> extends never ?
    EnumOnlyExtended<O, N>:
    ['Error: enum changed', O];

type ClosedEnumsDoNotChange = AllValues<{
  [K in keyof current.ClosedEnums & keyof old.ClosedEnums]:
      EnumIsEquivalent<old.ClosedEnums[K], current.ClosedEnums[K]>;
}>;
assertNever<ClosedEnumsDoNotChange>();

type CheckExtensibleEnums = AllValues<{
  [K in keyof current.ExtensibleEnums & keyof old.ExtensibleEnums]:
      EnumOnlyExtended<old.ExtensibleEnums[K], current.ExtensibleEnums[K]>;
}>;
assertNever<CheckExtensibleEnums>();
