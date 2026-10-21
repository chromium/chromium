---
name: base/propagate-test-overrides-to-native
rubric: correctness
severity: nit
---

**Rule.** In Java classes that mirror state to native C++ (such as DeviceInfo,
AndroidInfo, or ApkInfo), ensure test override setters (e.g., set\*ForTesting)
and reset methods propagate updated state to C++ via sendToNative(...) when
native code is loaded (sIsNativeLoaded). Do not leave native state out of sync
after mutating Java test override fields.

**Why.** When native code initializes, it reads and caches bridged system
attributes from Java. If a test subsequently overrides or resets a property in
Java without pushing the updated state to native, C++ and Java will observe
conflicting values during test execution.

## Examples

Bad:

```cpp
public static void setIsDesktopForTesting(boolean isDesktop) {
    sIsDesktopForTesting = isDesktop;
    ResettersForTesting.register(() -> sIsDesktopForTesting = null);
}
```

Good:

```cpp
public static void setIsDesktopForTesting(boolean isDesktop) {
    sIsDesktopForTesting = isDesktop;
    ResettersForTesting.register(() -> sIsDesktopForTesting = null);
    if (sIsNativeLoaded) {
        sendToNative(getInstance().mIDeviceInfo);
    }
}
```
