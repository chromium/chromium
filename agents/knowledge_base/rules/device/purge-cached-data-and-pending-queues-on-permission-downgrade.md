---
name: device/purge-cached-data-and-pending-queues-on-permission-downgrade
rubric: security
severity: blocking
---

**Rule.** When a device, sensor, or location permission is revoked or downgraded
(for example, from precise to approximate accuracy, or when a per-device or
origin permission grant is revoked), immediately purge all state tied to the
revoked privilege level:

1. Clear cached readings, positions, or shared-memory buffers so stale
   high-privilege data cannot be served to subsequent queries.
2. Stop active notification, advertisement, or discovery streams for unpermitted
   devices.
3. Remove pending or deferred operations for unpermitted devices—draining any
   queued Mojo response callbacks into a local container and invoking them with
   an explicit error status rather than dropping them uninvoked—in addition to
   closing active connections.

**Why.** Closing active connections or flipping a permission flag without
purging cached readings, active event streams, and queued asynchronous requests
allows pre-revocation data to leak after access is revoked, allows in-flight
operations queued before revocation to complete afterwards, or triggers fatal
Mojo debug crashes and hung renderer promises if pending response callbacks are
destroyed without being run.

## Examples

Bad:

```cpp
void GeolocationServiceImpl::OnPermissionStatusUpdated(
    blink::mojom::PermissionStatus status) {
  if (status != blink::mojom::PermissionStatus::GRANTED) {
    // BAD: Closes active receivers on full revocation, but when downgraded from
    // precise to approximate (still GRANTED), cached high-accuracy position
    // remains in `cached_position_` and is delivered to new queries.
    receiver_set_.Clear();
  }
}

void WebBluetoothServiceImpl::OnPermissionRevoked(const url::Origin& origin) {
  // BAD: Disconnects active GATT connections, but leaves in-flight device IDs
  // in `pending_connection_device_ids_`, active notify/advertisement sessions,
  // and queued callbacks in `characteristic_id_to_deferred_start_` (or
  // `.clear()`s the map without invoking pending Mojo callbacks).
  connected_devices_->CloseConnectionsToDevicesNotInList(permitted_ids);
}
```

Good:

```cpp
void GeolocationServiceImpl::OnPermissionResultUpdated(
    content::PermissionResult result) {
  if (result.status != blink::mojom::PermissionStatus::GRANTED) {
    cached_position_.reset();
    receiver_set_.Clear();
    return;
  }
  if (grant_level_ == GrantLevel::kPrecise &&
      result.grant_level == GrantLevel::kApproximate) {
    // Purge cached high-accuracy reading before switching to coarse mode.
    cached_position_.reset();
    grant_level_ = GrantLevel::kApproximate;
  }
}

void WebBluetoothServiceImpl::OnPermissionRevoked(const url::Origin& origin) {
  // Web Bluetooth permissions are keyed to the top-level main frame origin, and
  // fenced frames are disallowed at bind time. (For subframe-scoped APIs,
  // compare against render_frame_host().GetLastCommittedOrigin().)
  if (render_frame_host().GetOutermostMainFrame()->GetLastCommittedOrigin() !=
      origin) {
    return;
  }
  std::set<blink::WebBluetoothDeviceId> permitted_ids = GetPermittedDeviceIds();
  connected_devices_->CloseConnectionsToDevicesNotInList(permitted_ids);

  std::erase_if(pending_connection_device_ids_, [&](const auto& device_id) {
    return !permitted_ids.contains(device_id);
  });
  std::erase_if(watch_advertisements_clients_, [&](const auto& client) {
    return !permitted_ids.contains(client->device_id());
  });

  std::vector<RemoteCharacteristicStartNotificationsCallback> callbacks_to_fail;
  for (auto& [characteristic_id, session_data] :
       characteristic_id_to_notify_session_) {
    if (!permitted_ids.contains(session_data->device_id) &&
        session_data->start_notifications_callback) {
      callbacks_to_fail.push_back(
          std::move(session_data->start_notifications_callback));
    }
  }
  std::erase_if(characteristic_id_to_notify_session_, [&](const auto& pair) {
    return !permitted_ids.contains(pair.second->device_id);
  });

  for (auto& [characteristic_id, deferred_queue] :
       characteristic_id_to_deferred_start_) {
    if (!deferred_queue.empty() &&
        !permitted_ids.contains(deferred_queue.front()->device_id)) {
      while (!deferred_queue.empty()) {
        callbacks_to_fail.push_back(
            std::move(deferred_queue.front()->callback));
        deferred_queue.pop();
      }
    }
  }
  std::erase_if(characteristic_id_to_deferred_start_,
                [](const auto& pair) { return pair.second.empty(); });

  MaybeStopDiscovery();

  for (auto& callback : callbacks_to_fail) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::GATT_NOT_AUTHORIZED);
  }
}
```
