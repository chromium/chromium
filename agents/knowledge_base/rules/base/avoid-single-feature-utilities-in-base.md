---
name: base/avoid-single-feature-utilities-in-base
rubric: design
severity: should_fix
---

**Rule.** Do not add new utility classes, helper functions, test utilities, or
thin wrappers around standard types (in C++ or Java) to //base or //base/test
for single-feature or single-caller use cases, especially those tailored to
higher-layer packages (such as //extensions or //chrome). Keep helpers
co-located with the consuming feature or test file (or in //components if shared
across multiple consumers) until wide applicability across the project is
demonstrated.

**Why.** Code in //base carries a high bar and long-term maintenance overhead
across all of Chromium. Placing single-use utilities, test workarounds, or
narrow wrappers in //base or //base/test pollutes the core platform layer, leaks
higher-layer abstractions, and complicates future refactoring compared to
keeping utilities co-located with their callers.

## Examples

Bad:

```cpp
// In base/task/retry/retry_strategy.h:
namespace base {
class BASE_EXPORT RetryStrategy {
 public:
  RetryStrategy(int max_retries, base::TimeDelta delay);
  void Start(RetryableTask* task, base::OnceCallback<void(bool)> done);
};
}  // namespace base

// In base/test/android/content_uri_test_utils.h:
// Adding a single-caller test workaround tailored to //extensions:
std::optional<FilePath> CreateCacheCopyAndGetContentUri(
    const FilePath& source_path,
    const ScopedTempDir& temp_dir);
```

Good:

```cpp
// In chrome/browser/android/compositor/retry_strategy.h:
namespace chrome::android {
class RetryStrategy {
 public:
  RetryStrategy(int max_retries, base::TimeDelta delay);
  void Start(RetryableTask* task, base::OnceCallback<void(bool)> done);
};
}  // namespace chrome::android

// In chrome/browser/extensions/api/.../developer_private_api_unittest.cc:
// Keep the single-caller, extension-specific test helper local to the unit test.
base::FilePath CreateCacheCopyAndGetContentUri(
    const base::FilePath& source_path,
    const base::ScopedTempDir& temp_dir) {
  EXPECT_TRUE(base::CopyDirectory(source_path, temp_dir.GetPath(), true));
  return *base::test::android::GetInMemoryContentTreeUriFromCacheDirDirectory(
      temp_dir.GetPath().Append(source_path.BaseName()));
}
```
