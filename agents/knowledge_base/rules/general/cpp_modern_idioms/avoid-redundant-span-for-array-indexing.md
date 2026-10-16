---
name: general/avoid-redundant-span-for-array-indexing
rubric: readability
severity: should_fix
---

**Rule.** Use 'std::array\<T, N>' (or 'std::to_array') for fixed-size buffers
instead of declaring raw C-style arrays wrapped in 'base::span'. When indexing
or retrieving element addresses from 'std::array' or other standard containers,
index the container directly ('arr[i]' or '&arr[i]') rather than constructing
temporary 'base::span' wrappers or '.subspan()' views.

**Why.** 'std::array' natively provides safe, bounds-checked operations under
compiler hardening and unsafe-buffer analysis without triggering
-Wunsafe-buffer-usage warnings, and '.data()' can be passed directly to C-style
APIs. Wrapping raw C-style arrays or standard containers in 'base::span' or
calling '.subspan()' solely for indexed access adds boilerplate and indirection
without additional safety.

## Examples

Bad:

```cpp
// 1. Raw C-style array wrapped in base::span for indexing:
HANDLE handles[MAXIMUM_WAIT_OBJECTS];
base::span<HANDLE> handles_span(handles);
for (size_t i = 0; i < events.size(); ++i) {
  handles_span[i] = events[i]->handle();
}

// 2. Redundant base::span wrapper or subspan to index std::array:
auto names_span = base::span(log_severity_names);
return names_span[static_cast<size_t>(severity)];

base::span<unsigned char> buf_span(buf_);
auto subspan = buf_span.subspan(offset_);
linux_dirent* dirent = reinterpret_cast<linux_dirent*>(subspan.data());
```

Good:

```cpp
// 1. Use std::array directly and pass .data() to C APIs:
std::array<HANDLE, MAXIMUM_WAIT_OBJECTS> handles;
for (size_t i = 0; i < events.size(); ++i) {
  handles[i] = events[i]->handle();
}
WaitForMultipleObjects(count, handles.data(), FALSE, INFINITE);

// 2. Index the std::array directly:
return log_severity_names[static_cast<size_t>(severity)];

linux_dirent* dirent = reinterpret_cast<linux_dirent*>(&buf_[offset_]);
```
