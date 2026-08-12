---
name: sync-new-data-type
description: >-
  Adds scaffolding for a new Sync data type in Chromium across protocol buffers,
  DataType definitions, feature flags, controller builders, unit tests, and
  metrics.
---

# Add New Sync Data Type

This skill provides step-by-step guidance for adding the initial scaffold (CL
#1) for a new Sync data type in Chromium.

## Overview

Introducing a new Sync data type involves wiring up protocol buffers, enum
definitions, type info tables, feature flags, controller builders,
unit/integration tests, and histogram metrics.

This skill automates creating the scaffold CL by dynamically inspecting the
codebase for current `static_assert` counters and enum values.

## Inputs Required

Before starting, obtain the following inputs from the user:

1. **Data Type Name (`<DATA_TYPE>` in `UPPER_SNAKE_CASE`):** e.g., `FOO_BAR`,
   `PASSKEY_METADATA`
2. **Bug Number (`<BUG_NUMBER>`):** e.g., `123456789`

### Derived Casing Conventions

From `<DATA_TYPE>` (e.g., `FOO_BAR`), compute the following variations:

- **`UPPER_SNAKE_CASE`**: `<DATA_TYPE>` (e.g., `FOO_BAR`)
- **`lower_snake_case`**: `<lower_snake_case>` (e.g., `foo_bar`)
- **`CamelCase` / `PascalCase`**: `<CamelCase>` (e.g., `FooBar`)
- **`Title Case`**: `<Title Case>` (e.g., `Foo Bar`)

______________________________________________________________________

## Instructions

Inspect the existing codebase dynamically to discover current `static_assert`
counters and enum values, then make all necessary scaffold changes across the
following files:

### 1. Protocol Buffers & Build Rules

1. **`components/sync/protocol/<lower_snake_case>_specifics.proto`** (Create New
   File): Create this file with standard Chromium boilerplate:

   ```protobuf
   // Copyright 2026 The Chromium Authors
   // Use of this source code is governed by a BSD-style license that can be
   // found in the LICENSE file.

   // If you change or add any fields in this file, update proto_visitors.h and
   // potentially proto_enum_conversions.{h, cc}.

   syntax = "proto2";

   option java_multiple_files = true;
   option java_package = "org.chromium.components.sync.protocol";

   option optimize_for = LITE_RUNTIME;

   package sync_pb;

   // FIXME (in this CL): document.
   message <CamelCase>Specifics {
     // TODO(crbug.com/<BUG_NUMBER>): In CL #2, add fields that you wish to sync, then
     // update proto_visitors.h and potentially proto_enum_conversions.*.
   }
   ```

2. **`components/sync/protocol/entity_specifics.proto`**:

   - Add import in alphabetical order:
     `import "components/sync/protocol/<lower_snake_case>_specifics.proto";`
   - Per the guidelines in `entity_specifics.proto`, calculate `<FIELD_NUMBER>`
     by selecting a `Cr-Commit-Position` of a past commit authored by the
     developer (e.g., query
     `git log -1 --author="$(git config user.email)" --grep="Cr-Commit-Position" origin/main`
     and extract the commit position number). If the developer has no prior
     landed commits, ask the user or pick an available position from
     `origin/main`.
   - Verify that `<FIELD_NUMBER>` does not collide with any existing field tags
     or `reserved` numbers in `entity_specifics.proto`.
   - In `message EntitySpecifics` under `oneof specifics_variant`, add the field
     right before the "When adding a new type" comment with a `FIXME` comment:
     ```protobuf
     // FIXME (in this CL): Verify tag number matches a valid Cr-Commit-Position.
     <CamelCase>Specifics <lower_snake_case> = <FIELD_NUMBER>;
     ```

3. **`components/sync/protocol/protocol_sources.gni`**:

   - Add `"<lower_snake_case>_specifics.proto",` in alphabetical order to
     `sync_protocol_sources`.

4. **`components/sync/protocol/proto_visitors.h`**:

   - Add `#include "components/sync/protocol/<lower_snake_case>_specifics.pb.h"`
     in alphabetical order to the list of includes at the top.
   - Increment `static_assert(N == GetNumDataTypes(), ...)` in
     `VISIT_PROTO_FIELDS(const sync_pb::EntitySpecifics& proto)` from `N` to
     `N + 1`.
   - Add `VISIT(<lower_snake_case>);` to
     `VISIT_PROTO_FIELDS(const sync_pb::EntitySpecifics& proto)`.
   - Add the empty visitor function:
     ```cpp
     VISIT_PROTO_FIELDS(const sync_pb::<CamelCase>Specifics& proto) {
       // TODO(crbug.com/<BUG_NUMBER>): In CL #2, VISIT fields added to specifics.
     }
     ```

5. **`components/sync/protocol/proto_value_conversions_unittest.cc`**:

   - Increment `static_assert(N == syncer::GetNumDataTypes(), ...)` from `N` to
     `N + 1`.
   - Add `DEFINE_SPECIFICS_TO_VALUE_TEST(<lower_snake_case>)` within the
     keep-sorted macro list in alphabetical order.

______________________________________________________________________

### 2. Core DataType & Feature Declarations

6. **`components/sync/base/data_type.h`**:

   - In `enum DataType`:
     - Add `// FIXME (in this CL): document.` and `<DATA_TYPE>,` right before
       `LAST_USER_DATA_TYPE`.
     - Update `LAST_USER_DATA_TYPE = <DATA_TYPE>,`.
   - In `enum class DataTypeForHistograms`:
     - Inspect the current maximum integer value `M` assigned to the last entry
       before `kMaxValue`.
     - Add `k<CamelCase> = <M + 1>,` and update `kMaxValue = k<CamelCase>,`.

7. **`components/sync/base/data_type.cc`**:

   - In `kDataTypeInfoTable`: Add the new entry struct:
     ```cpp
     {
         .type = <DATA_TYPE>,
         .specifics_field_number =
             sync_pb::EntitySpecifics::k<CamelCase>FieldNumber,
         .debug_string = "<Title Case>",
         .histogram_suffix = "<DATA_TYPE>",
         .stable_lowercase_string = "<lower_snake_case>",
         // FIXME (in this CL): Verify encryption_policy (e.g.,
         // kEncryptedIfCustomPassphraseSet vs. kAlwaysEncrypted vs.
         // kNeverEncrypted) with the sync champion.
         .encryption_policy =
             EncryptionPolicy::kEncryptedIfCustomPassphraseSet,
         .priority = DataTypePriority::kRegular,
         // FIXME (in this CL): Verify communication_direction (e.g.,
         // kRegularTwoWay vs. kCommitOnly) with the sync champion.
         .communication_direction = CommunicationDirection::kRegularTwoWay,
         .apply_updates_batch_policy = ApplyUpdatesBatchPolicy::kStandard,
         .unsynced_data_check_on_signout_policy =
             UnsyncedDataCheckOnSignoutPolicy::kNone,
         .cross_user_sharing_policy = CrossUserSharingPolicy::kNone,
         .local_sync_support_policy = LocalSyncSupportPolicy::kUnsupported,
     },
     ```
   - Increment `static_assert(GetNumDataTypes() == N, ...)` from `N` to `N + 1`.
   - In `AddDefaultFieldValue()`: Add
     `case <DATA_TYPE>: specifics->mutable_<lower_snake_case>(); break;`.
   - In `DataTypeHistogramValue()`: Add
     `case <DATA_TYPE>: return DataTypeForHistograms::k<CamelCase>;`.

8. **`components/sync/base/features.h` & `components/sync/base/features.cc`**:

   - In `features.h`:
     ```cpp
     // FIXME (in this CL): If you already have a flag, delete this and use yours.
     // Otherwise, document.
     BASE_DECLARE_FEATURE(kSync<CamelCase>);
     ```
   - In `features.cc`:
     ```cpp
     BASE_FEATURE(kSync<CamelCase>, base::FEATURE_DISABLED_BY_DEFAULT);
     ```

9. **`components/sync/engine/cycle/data_type_tracker.cc`**:

   - In `GetDefaultLocalChangeNudgeDelay()`: Add `case <DATA_TYPE>:` (returning
     `kMediumLocalChangeNudgeDelay`).
   - In `CanGetCommitsFromExtensions()`: Add `case <DATA_TYPE>:` (returning
     `false`).

______________________________________________________________________

### 3. Controller Builder & Settings

10. **`components/browser_sync/common_controller_builder.h` &
    `components/browser_sync/common_controller_builder.cc`**:

    - In `common_controller_builder.h`: Declare
      `std::unique_ptr<syncer::DataTypeController> Create<CamelCase>DataTypeController();`.
    - In `common_controller_builder.cc`:
      - In `Build()`:
        ```cpp
        if (!disabled_types.Has(syncer::<DATA_TYPE>)) {
          add_controller(Create<CamelCase>DataTypeController());
        }
        ```
      - Add implementation:
        ```cpp
        std::unique_ptr<syncer::DataTypeController>
        CommonControllerBuilder::Create<CamelCase>DataTypeController() {
          if (!base::FeatureList::IsEnabled(syncer::kSync<CamelCase>)) {
            return nullptr;
          }

          // FIXME (in this CL): If your data type will *eventually* sync in both iOS
          // and non-iOS platforms, keep the TODO below here. Otherwise, move it
          // to CreateDataTypeControllers() in ChromeSyncClient or
          // IOSChromeSyncClient. If the type will eventually sync in all platforms,
          // but will first be launched in a single one, keep the TODO here.
          //
          // TODO(crbug.com/<BUG_NUMBER>): In CL #4, register the type, i.e. instantiate
          // the DataTypeController. There is more than one way to go about it,
          // but one option is:
          // - Create a trivial implementation of DataTypeSyncBridge which lives in
          //   your feature's directory. It should have synchronous access to your
          //   data model (e.g. DualReadingListModel) and be (indirectly) owned by a
          //   CoolKeyedService (often the model itself).
          // - Expose CoolKeyedService::GetControllerDelegate() which calls
          //   bridge->change_processor()->GetControllerDelegate().
          // - Inject CoolKeyedService in this class and call GetControllerDelegate()
          //   on it to create the DataTypeController.
          // In CLs #5, #6, ..., implement the bridge and keep adding unit tests.
          return nullptr;
        }
        ```

11. **`components/sync/base/user_selectable_type.cc`**:

    - In `GetUserSelectableTypeInfo()`:
      - Increment `static_assert(N == syncer::GetNumDataTypes(), ...)` from `N`
        to `N + 1`.
      - Add:
        ```cpp
        // TODO(crbug.com/<BUG_NUMBER>): In CL #3, map <DATA_TYPE> to an existing selectable
        // type or to a new one. The first option should be trivial, the second
        // requires touching UI code across platforms.
        ```

12. **`components/sync/base/user_selectable_type_unittest.cc`**:

    - In `AmbiguousTypes()`:
      ```cpp
      // TODO(crbug.com/<BUG_NUMBER>): In CL #3, map <DATA_TYPE> to an existing
      // selectable type or to a new one and remove it from here (unless it's
      // ambiguous).
      data_types.Put(<DATA_TYPE>);
      ```

13. **`components/sync/service/sync_user_settings_impl_unittest.cc`**:

    - In `TEST_F(SyncUserSettingsImplTest, PreferredTypesSyncEverything)`:
      ```cpp
      // TODO(crbug.com/<BUG_NUMBER>): In CL #3, delete (<DATA_TYPE> is now mapped to a
      // selectable type).
      expected_types.Remove(<DATA_TYPE>);
      ```
    - In `TEST_F(SyncUserSettingsImplTest, PreferredOsTypesSyncAllOsTypes)`
      (under `#if BUILDFLAG(IS_CHROMEOS)`):
      ```cpp
      // TODO(crbug.com/<BUG_NUMBER>): In CL #3, delete (<DATA_TYPE> is now mapped to a
      // selectable type).
      expected_types.Remove(<DATA_TYPE>);
      ```

______________________________________________________________________

### 4. Tests & Metrics

14. **`chrome/browser/sync/sync_service_factory_unittest.cc`**:

    - In `DefaultDatatypes()`:
      - Increment `static_assert(N == syncer::GetNumDataTypes(), ...)` from `N`
        to `N + 1`.
      - Add:
        ```cpp
        if (base::FeatureList::IsEnabled(syncer::kSync<CamelCase>)) {
          datatypes.Put(syncer::<DATA_TYPE>);
        }
        ```

15. **`ios/chrome/browser/sync/model/sync_service_factory_unittest.mm`**:

    - In `DefaultDatatypes()`:
      - Increment `static_assert(N == syncer::GetNumDataTypes(), ...)` from `N`
        to `N + 1`.
      - Add:
        ```cpp
        if (base::FeatureList::IsEnabled(syncer::kSync<CamelCase>)) {
          datatypes.Put(syncer::<DATA_TYPE>);
        }
        ```

16. **`chrome/browser/sync/test/integration/sync_test.cc`**:

    - In `AllowedTypesInStandaloneTransportMode()`:
      - Increment `static_assert(N == syncer::GetNumDataTypes(), ...)` from `N`
        to `N + 1`.
      - Add:
        ```cpp
        if (base::FeatureList::IsEnabled(syncer::kSync<CamelCase>)) {
          allowed_types.Put(syncer::<DATA_TYPE>);
        }
        ```

17. **`tools/metrics/histograms/metadata/sync/enums.xml`**:

    - In `<enum name="SyncDataTypes">`:
      - Add `<int value="<M + 1>" label="<Title Case>"/>` matching the histogram
        enum integer added in `data_type.h`.

18. **`tools/metrics/histograms/metadata/sync/histograms.xml`**:

    - In `<variants name="SyncDataType">` (or DataType suffix variants):
      - Add `<variant name=".<DATA_TYPE>" summary="<DATA_TYPE>"/>` in
        alphabetical order.

______________________________________________________________________

### 5. Verification & Commit Message

1. Verify that all updated `static_assert` statements match
   `syncer::GetNumDataTypes()`.
2. Format git commit message as:

```text
[Sync] Add scaffold for DataType::<DATA_TYPE>

Adds the new DataType enum value and an empty specifics proto for
DataType::<DATA_TYPE>.

Follow-up CL roadmap:
- CL #2: Add fields to <lower_snake_case>_specifics.proto and proto_visitors.h.
- CL #3: Map <DATA_TYPE> to a UserSelectableType in user_selectable_type.cc.
- CL #4: Register DataTypeController in CommonControllerBuilder.
- CL #5+: Implement DataTypeSyncBridge and integration tests.

Bug: <BUG_NUMBER>
```

3. **Post-generation Review**: Prompt the user to review the generated code and
   address all `FIXME (in this CL)` markers:
   - Field number in `components/sync/protocol/entity_specifics.proto`.
   - DataType policies (`encryption_policy`, `communication_direction`, etc.) in
     `components/sync/base/data_type.cc`.
   - Feature flag declaration in `components/sync/base/features.h`.
   - Controller builder registration in
     `components/browser_sync/common_controller_builder.cc`.
