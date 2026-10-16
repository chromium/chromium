---
name: general/avoid-redundant-test-accessors-in-public-api
rubric: design
severity: should_fix
---

**Rule.** Do not add test-only query helpers, duplicate methods, redundant
accessors, or internal offsets to production classes when the functionality is
only needed by unit tests (such as evaluating test restrictions or environment
setup) or when existing public APIs already provide sufficient data. Keep test
calculations within test fixtures or relocate test-only query helpers to
dedicated test utility classes or testonly targets.

**Why.** Exposing test-only query helpers, duplicate methods, or internal
offsets on production classes pollutes public APIs, leaks internal details, and
creates confusion when callers must choose between production and test-only
methods (e.g., methods ignoring switch or flag overrides). Keeping test logic in
test utilities or test fixtures preserves clean encapsulation and prevents
production code from accidentally calling test-only logic.

## Examples

Bad:

```cpp
// 1. Redundant C++ offset accessor in production header (pe_image_reader.h):
class BASE_EXPORT PeImageReader {
 public:
  const IMAGE_SECTION_HEADER* GetSectionHeaderAt(size_t index);
  size_t GetSectionHeaderOffsetAt(size_t index);  // Added solely for a test
};

// 2. Test-only query helper in production Java class (DeviceInfo.java):
public static boolean isDesktop() { return getInstance().mIDeviceInfo.isDesktop; }
public static boolean isDesktopFreeform() {  // Only needed by test restriction checks
    return BuildConfig.IS_DESKTOP_ANDROID && getPackageManager().hasSystemFeature(FEATURE_PC);
}
```

Good:

```cpp
// 1. Calculate test offsets in test fixture using existing public APIs:
const IMAGE_SECTION_HEADER* last_header =
    image_reader_.GetSectionHeaderAt(image_reader_.GetNumberOfSections() - 1);
size_t header_size = reinterpret_cast<uintptr_t>(last_header) + sizeof(*last_header) -
                     reinterpret_cast<uintptr_t>(data_file_.data());

// 2. Isolate test-only query helpers in test utilities (UiRestriction.java):
static boolean isDesktopFreeform() {
    return BuildConfig.IS_DESKTOP_ANDROID && getPackageManager().hasSystemFeature(FEATURE_PC);
}
```
