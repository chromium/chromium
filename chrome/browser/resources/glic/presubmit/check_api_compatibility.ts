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

// Get the set of TypesConsumedByClient in both old and current. This
// allows us to ignore types removed from TypesConsumedByClient.
type OldTypesConsumedByClient = {
  [K in keyof old.TypesConsumedByClient &
   keyof current.TypesConsumedByClient]: old.TypesConsumedByClient[K]
};
type OldTypesConsumedByHost = {
  [K in keyof old.TypesConsumedByHost &
   keyof current.TypesConsumedByHost]: old.TypesConsumedByHost[K]
};

type CurrentTypesConsumedByClient = {
  [K in keyof old.TypesConsumedByClient &
   keyof current.TypesConsumedByClient]: current.TypesConsumedByClient[K]
};

type CurrentTypesConsumedByHost = {
  [K in keyof old.TypesConsumedByHost &
   keyof current.TypesConsumedByHost]: current.TypesConsumedByHost[K]
};

/*
These are the kinds of changes we might see, and how they're categorized.

* OK in host and client types:
  * Adding an optional field. {x:number} --> {x:number; y?: string}
  * Adding an optional parameter. foo():void -> foo(x?:number)

* ERROR in host and client types.
  * Removing any field.       {x?:number} --> {}
  * Adding a required field.  {} --> {x:number}
  * Adding a required parameter. foo():void -> foo(x:number)
  * Widening a field type.    {x:number} --> {x:number|string}
     (Changing a host type in this way is likely not compatible for old versions
      of Chrome.)

In summary, host and client types have the same compatibility requirements.
TODO(harringtond): We should just merge these two concepts.
*/

// Note: We're just using assignment to verify these types are compatible.

export const oldTypesAreCompatibleWithCurrent: CurrentTypesConsumedByHost&
    CurrentTypesConsumedByClient =
        null as any as old.TypesConsumedByHost & old.TypesConsumedByClient;
export const currentTypesAreCompatibleWithOld: OldTypesConsumedByClient&
    CurrentTypesConsumedByHost = null as any as current.TypesConsumedByHost &
    current.TypesConsumedByClient;

// Make all fields required, then check that all fields are compatible. This
// ensures we don't remove optional fields.
export const canNotRemoveAnythingFromClientTypes:
    DeepRequired<OldTypesConsumedByClient> =
        null as any as DeepRequired<current.TypesConsumedByClient>;

export const canNotRemoveAnythingFromHostTypes:
    DeepRequired<OldTypesConsumedByHost> =
        null as any as DeepRequired<old.TypesConsumedByHost>;

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
