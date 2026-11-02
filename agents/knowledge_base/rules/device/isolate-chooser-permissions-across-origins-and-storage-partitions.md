---
name: device/isolate-chooser-permissions-across-origins-and-storage-partitions
rubric: correctness
severity: blocking
---

**Rule.** In device permission contexts and browser delegates that manage
persistent and ephemeral chooser grants:

1. Validate that persisted permission objects contain all required non-empty
   identifier fields, and never synthesize placeholder serial numbers or
   identifiers to force an ephemeral device into persistent storage.
2. Scope ephemeral grants and device-lookup mappings per `(origin, device)` so
   revoking access for one device or one origin does not wipe sibling devices
   granted to the same origin, orphan shared device tokens held by other
   origins, or evict connected hardware from the profile-wide device inventory.
3. When cleaning up active device connections or permission state across a
   profile, iterate across all loaded storage partitions rather than inspecting
   only the default storage partition.

**Why.** Checking only whether an origin still has any entry in a permission map
misses single-device revocations when the origin retains grants for other
devices; erasing shared device tokens or hardware inventory entries globally on
single-origin revocation breaks access for other permitted origins; and
inspecting only the default storage partition leaves active connections open in
isolated partitions such as guest views and Isolated Web Apps.

## Examples

Bad:

```cpp
void SerialChooserContext::RevokePortPermissionWebInitiated(
    const url::Origin& origin,
    const base::UnguessableToken& token) {
  // BAD: Globally erases `token` from `persistent_ports_by_token_`, breaking
  // revocation for any other origin that also holds permission for `token`.
  auto it = persistent_ports_by_token_.find(token);
  if (it != persistent_ports_by_token_.end()) {
    RevokeObjectPermissionInternal(origin, it->second);
    persistent_ports_by_token_.erase(it);
  }
}

void CloseConnectionsForProfile(content::BrowserContext* context) {
  // BAD: Misses non-default StoragePartitions (<webview>, Isolated Web Apps).
  auto* partition = context->GetDefaultStoragePartition();
  ClosePartitionConnections(partition);
}
```

Good:

```cpp
void SerialChooserContext::RevokePortPermissionWebInitiated(
    const url::Origin& origin,
    const base::UnguessableToken& token) {
  auto it = persistent_ports_by_token_.find(token);
  if (it == persistent_ports_by_token_.end()) {
    return;
  }
  // Copy the persistent port dictionary before RevokeObjectPermissionInternal
  // mutates internal maps, and only evict `token` from the persistent-port
  // lookup cache if no other origin holds a grant (while connected hardware
  // enumeration caches remain bound to physical device connection state).
  base::Value::Dict object = it->second.Clone();
  RevokeObjectPermissionInternal(origin, object);
  if (!HasPortPermissionForAnyOrigin(token)) {
    persistent_ports_by_token_.erase(token);
  }
}

void CloseConnectionsForProfile(content::BrowserContext* context) {
  context->ForEachLoadedStoragePartition(
      [](content::StoragePartition* partition) {
        ClosePartitionConnections(partition);
      });
}
```
