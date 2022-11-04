This directory exposes the NearbyConnectionsManager interface as a separate
build target so that `//chrome/browser/ash/login/oobe_quick_start` may depend
on it without creating a dependency cycle.

```
//chrome/browser, nearby_sharing
  |                     |
  v                     |
//chrome/browser/ash    |
  |                     |
  v                     v
oobe_quick_start -----> //chrome/browser/nearby_sharing/public/cpp
```

This is intended as a temporary solution. We plan to refactor the code in
`nearby_sharing` and extract NearbyConnectionsManager,
NearbyConnectionsManagerImpl, and any other code useful outside of the context
of Nearby Share into a reuseable component.
