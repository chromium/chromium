---
name: general/avoid-eager-startup-settings-queries
rubric: safety
severity: should_fix
---

**Rule.** Do not perform synchronous Binder IPCs (such as querying
Settings.Global, Settings.Secure, or Settings.System via ContentResolver) during
Android startup initialization. Expose on-demand getters (such as a separate
@CalledByNative accessor) or query settings lazily when requested to avoid
startup latency and stale cached values.

**Why.** Querying Android system settings via ContentResolver executes
synchronous Binder IPCs to system services, adding latency to the application
launch path. Furthermore, mutable device settings can change at runtime, so
caching them during startup risks serving stale data.

## Examples

Bad:

```cpp
// Inside startup initialization of DeviceInfo:
mDeviceInfo.deviceName =
        Settings.Global.getString(
                ContextUtils.getApplicationContext().getContentResolver(), "device_name");
```

Good:

```cpp
// On-demand getter queried only when requested by native or callers:
@CalledByNative
public static String getDeviceName() {
    return Settings.Global.getString(
            ContextUtils.getApplicationContext().getContentResolver(), "device_name");
}
```
