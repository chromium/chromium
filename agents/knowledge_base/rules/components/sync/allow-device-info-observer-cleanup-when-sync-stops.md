---
name: components/sync/allow-device-info-observer-cleanup-when-sync-stops
description: Use when implementing DeviceInfoTracker::Observer::OnDeviceInfoChange(), especially if it early-returns on !IsSyncing(), which skips cleanup of device caches when sync stops.
rubric: correctness
severity: should_fix
---

**Rule.** When implementing
syncer::DeviceInfoTracker::Observer::OnDeviceInfoChange(), do not
unconditionally early-return when !tracker->IsSyncing(). Allow observers
maintaining local device caches or remote state to proceed with cleanup and
garbage collection when sync stops and device lists are cleared.

**Why.** OnDeviceInfoChange() is notified not only when new devices sync or
initialize, but also when sync stops and the device info list is wiped.
Early-returning solely on !tracker->IsSyncing() prevents observers from clearing
local caches or garbage collecting stale state associated with previously active
remote devices.

## Examples

Bad:

```cpp
void CrossDevicePrefTrackerImpl::OnDeviceInfoChange() {
  if (!tracker_ || !tracker_->IsSyncing()) {
    return;
  }
  HandleRemoteDeviceInfoChanges();
  GarbageCollectStaleCacheGuids();
}
```

Good:

```cpp
void CrossDevicePrefTrackerImpl::OnDeviceInfoChange() {
  if (!tracker_) {
    return;
  }
  if (!tracker_->IsSyncing() && active_device_guids_.empty()) {
    return;
  }
  HandleRemoteDeviceInfoChanges();
  GarbageCollectStaleCacheGuids();
}
```
