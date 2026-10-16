---
name: general/avoid-global-form-factor-test-setters-in-device-info
rubric: readability
severity: should_fix
---

**Rule.** Do not add global `*_for_testing` setters or static override fields to
`base::android::device_info` or `org.chromium.base.DeviceInfo` to simulate
device form factors. When testing form-factor-specific behavior (such as
desktop, automotive, or foldable layouts), follow
`docs/ui/android/device_form_factor.md` by using established test mechanisms
(such as `OverrideContextWrapperTestRule`, `@Restriction`, or
`--force-desktop-android`) or parameterizing behavior via client dependency
injection.

**Why.** Adding ad-hoc global test setters to core device detection utilities
introduces mutable process-global state and bypasses existing test harness
architecture. Utilizing standard test rules like
`OverrideContextWrapperTestRule` or client dependency injection keeps production
device information immutable and prevents test state leakage across test suites.

## Examples

Bad:

```cpp
// In base/android/device_info.h:
BASE_EXPORT void set_is_desktop_for_testing(bool is_desktop);
BASE_EXPORT void reset_is_desktop_for_testing();

// In org/chromium/base/DeviceInfo.java:
public static void setIsDesktopForTesting(boolean isDesktop) {
  sIsDesktopForTesting = isDesktop;
}
```

Good:

```cpp
// Parameterize form factor via client dependency injection:
class BookmarkClient {
 public:
  virtual BookmarkFormFactor GetBookmarkFormFactor() const = 0;
};

// Or in Android unit/instrumentation tests, use established test rules:
// @Rule
// public OverrideContextWrapperTestRule mOverrideContextWrapperTestRule =
//     new OverrideContextWrapperTestRule();
```
