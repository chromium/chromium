---
name: device/mutate-state-and-move-queues-before-reentrant-callbacks
rubric: safety
severity: blocking
---

**Rule.** Complete all internal state mutations (removing entries from tracking
containers, resetting handles, updating connection state flags) and move pending
callbacks or callback queues into stack-local variables **before** invoking Mojo
response callbacks, observer notifications, or asynchronous completion
callbacks. Keep container-erasure predicates (`std::erase_if`) free of side
effects: never invoke observer notifications or callbacks from inside an erasure
predicate.

**Why.** Invoking a Mojo callback, observer notification, or completion callback
can synchronously re-enter the object, close connections, or destroy `this` (for
example, by detaching the hosting frame or tearing down a client). Running
callbacks while internal maps are only partially updated or while a container is
mid-iteration causes re-entrant callers to observe inconsistent state,
invalidates iterators, or triggers heap-use-after-free when post-callback code
accesses member variables.

## Examples

Bad:

```cpp
void HidChooserContext::RevokeEphemeralDevicePermission(
    const url::Origin& origin,
    const device::mojom::HidDeviceInfo& device) {
  auto it = ephemeral_devices_.find(origin);
  if (it == ephemeral_devices_.end()) {
    return;
  }
  // BAD: Notifies observers from inside `std::erase_if` while `it->second` is
  // mid-iteration and before empty origin sets are pruned from
  // `ephemeral_devices_`.
  std::erase_if(it->second, [&](const std::string& guid) {
    auto* device_ptr = base::FindPtrOrNull(devices_, guid);
    if (device_ptr &&
        device_ptr->physical_device_id == device.physical_device_id) {
      NotifyPermissionRevoked(origin);
      return true;
    }
    return false;
  });
}

void WebViewPermissionHelper::SetPermission(int request_id, bool allowed) {
  auto it = pending_permission_requests_.find(request_id);
  // BAD: Invoking `callback` can synchronously destroy `this` (e.g. detaching
  // the guest frame), causing `pending_permission_requests_.erase(it)` to UAF.
  std::move(it->second.callback).Run(allowed);
  pending_permission_requests_.erase(it);
}
```

Good:

```cpp
void HidChooserContext::RevokeEphemeralDevicePermission(
    const url::Origin& origin,
    const device::mojom::HidDeviceInfo& device) {
  auto it = ephemeral_devices_.find(origin);
  if (it == ephemeral_devices_.end()) {
    return;
  }
  std::set<std::string>& device_guids = it->second;
  const bool revoked_permission =
      std::erase_if(device_guids, [&](const std::string& guid) {
        auto* device_ptr = base::FindPtrOrNull(devices_, guid);
        return device_ptr &&
               device_ptr->physical_device_id == device.physical_device_id;
      }) > 0;
  if (device_guids.empty()) {
    ephemeral_devices_.erase(it);
  }
  if (revoked_permission) {
    NotifyPermissionRevoked(origin);
  }
}

void WebViewPermissionHelper::SetPermission(int request_id, bool allowed) {
  auto it = pending_permission_requests_.find(request_id);
  if (it == pending_permission_requests_.end()) {
    return;
  }
  PermissionResponseCallback callback = std::move(it->second.callback);
  pending_permission_requests_.erase(it);
  std::move(callback).Run(allowed);
}
```
