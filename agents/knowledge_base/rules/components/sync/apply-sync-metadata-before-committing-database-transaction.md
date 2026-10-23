---
name: components/sync/apply-sync-metadata-before-committing-database-transaction
description: Use when a DataTypeSyncBridge records metadata changes in an in-memory MetadataChangeList (e.g. via Put() or Delete()), applies them with ApplyMetadataChanges(), or commits a database transaction.
rubric: correctness
severity: should_fix
---

**Rule.** In `syncer::DataTypeSyncBridge` implementations using in-memory
`MetadataChangeList`s, ensure recorded metadata changes (such as from
`change_processor()->Put()` or `Delete()` in local change handlers) are
explicitly persisted (e.g., via `ApplyMetadataChanges()`) before the change list
destructs. When persisting metadata within database transactions, apply all
metadata changes before committing the transaction. If the change list is
transferred early to a `SyncMetadataStoreChangeList`, perform any subsequent
metadata mutations on the active store change list rather than the moved-from
pointer.

**Why.** When a sync bridge uses an in-memory MetadataChangeList, Put() and
Delete() record metadata modifications only in memory; dropping the list without
calling ApplyMetadataChanges() loses dirty tracking and prevents syncing offline
changes. Furthermore, metadata changes must be flushed before committing
database transactions to ensure crash consistency, and early transfers consume
the unique pointer, making subsequent operations on the moved-from object
undefined.

## Examples

Bad:

```cpp
// Anti-pattern 1: Omitting ApplyMetadataChanges for local Put/Delete changes.
std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
    CreateMetadataChangeList();
change_processor()->Put(
    *change.key(), CreateEntityData(*change.data()), metadata_change_list.get());
// metadata_change_list goes out of scope and drops recorded changes.

// Anti-pattern 2: Committing transaction before applying metadata.
web_data_backend_->CommitChanges();
if (transaction) {
  transaction->Commit();
}
ApplyMetadataChanges(std::move(metadata_change_list));

// Anti-pattern 3: Mutating moved-from metadata_change_list after early transfer.
if (auto error = ApplyMetadataChanges(std::move(metadata_change_list))) {
  return error;
}
for (const auto& change : entity_changes) {
  metadata_change_list->ClearMetadata(remote.guid());
}
```

Good:

```cpp
// Pattern 1: Persist local in-memory metadata changes before destruction.
std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
    CreateMetadataChangeList();
change_processor()->Put(
    *change.key(), CreateEntityData(*change.data()), metadata_change_list.get());
ApplyMetadataChanges(std::move(metadata_change_list));

// Pattern 2: Apply metadata changes within the open transaction before commit.
if (std::optional<syncer::ModelError> error =
        ApplyMetadataChanges(std::move(metadata_change_list))) {
  return error;
}
web_data_backend_->CommitChanges();
if (transaction) {
  transaction->Commit();
}

// Pattern 3: Perform subsequent mutations on active SyncMetadataStoreChangeList.
std::unique_ptr<syncer::SyncMetadataStoreChangeList> store_change_list =
    ApplyMetadataChanges(std::move(metadata_change_list));
for (const auto& change : entity_changes) {
  store_change_list->ClearMetadata(remote.guid());
}
```
