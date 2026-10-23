---
name: components/sync/check-is-tracking-metadata-before-getting-remote-specifics
description: Use when a DataTypeSyncBridge calls GetPossiblyTrimmedRemoteSpecifics() or guards change_processor() calls such as Put() or Delete() with IsTrackingMetadata().
rubric: correctness
severity: blocking
---

**Rule.** When retrieving remote proto specifics via
change_processor()->GetPossiblyTrimmedRemoteSpecifics(...) in a
DataTypeSyncBridge, verify that metadata tracking is active
(change_processor()->IsTrackingMetadata()) beforehand to prevent fatal crashes.
Conversely, do not guard change_processor()->Put() or Delete() calls with
IsTrackingMetadata(); the change processor checks tracking internally and safely
no-ops if metadata is inactive.

**Why.** DataTypeLocalChangeProcessor::GetPossiblyTrimmedRemoteSpecifics()
contains a `CHECK(entity_tracker_)` assertion internally and crashes immediately
if invoked when metadata tracking is uninitialized or disabled. In contrast,
Put() and Delete() safely check `entity_tracker_` internally and ignore
mutations before initial sync completion, making call-site IsTrackingMetadata()
guards redundant.

## Examples

Bad:

```cpp
// Bad: Missing IsTrackingMetadata guard before GetPossiblyTrimmedRemoteSpecifics (crashes if uninitialized).
sync_pb::SavedTabGroupSpecifics specifics =
    change_processor()
        ->GetPossiblyTrimmedRemoteSpecifics(tab.guid())
        .saved_tab_group();

// Bad: Redundant IsTrackingMetadata guard before Put/Delete (processor handles this internally).
if (change_processor()->IsTrackingMetadata()) {
  change_processor()->Put(storage_key, std::move(entity),
                          batch->GetMetadataChangeList());
}
```

Good:

```cpp
// Good: Guard GetPossiblyTrimmedRemoteSpecifics with IsTrackingMetadata().
sync_pb::SavedTabGroupSpecifics specifics;
if (change_processor()->IsTrackingMetadata()) {
  specifics = change_processor()
                  ->GetPossiblyTrimmedRemoteSpecifics(tab.guid())
                  .saved_tab_group();
}

// Good: Call Put/Delete directly without redundant guards.
change_processor()->Put(storage_key, std::move(entity),
                        batch->GetMetadataChangeList());
```
