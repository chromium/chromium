---
name: general/prefer-overloads-over-type-suffixed-names
rubric: design
severity: should_fix
---

**Rule.** When an API performs the same conceptual operation across different
argument types, qualifiers, or optional features, prefer providing an overloaded
function or extending the existing function signature (e.g., using default
arguments) rather than inventing separate type-suffixed or feature-suffixed
names (such as `copy_from_volatile()` or `FooWith<Feature>()`). Reserve distinct
function names for operations that have genuine semantic differences or distinct
caller contracts.

**Why.** Encoding argument types, cv-qualifiers, or optional feature flags into
function names fragments the public API surface, introduces redundant wrappers,
and increases cognitive load. Consolidating variants under the canonical
function name via overloading or parameter extension keeps interfaces clean,
generic, and discoverable.

## Examples

Bad:

```cpp
// Bad: inventing type-suffixed names for type/qualifier variants:
constexpr void copy_from_volatile(
    span<const volatile element_type, extent> other);

// Bad: inventing '...With<Feature>' variants instead of extending the API:
BASE_EXPORT File CreateAndOpenTemporaryFileInDir(const FilePath& dir,
                                                 FilePath* temp_file);
BASE_EXPORT File CreateAndOpenTemporaryFileInDirWithPrefix(
    const FilePath& dir,
    const FilePath::StringType& name_prefix,
    FilePath* temp_file);
```

Good:

```cpp
// Good: overload the canonical function name across argument types:
template <typename U>
  requires(!std::is_const_v<element_type> &&
           std::is_same_v<U, const volatile element_type>)
constexpr void copy_from(span<U, extent> other);

// Good: extend existing function with defaulted or optional parameters:
BASE_EXPORT File CreateAndOpenTemporaryFileInDir(
    const FilePath& dir,
    FilePath* temp_file,
    FilePath::StringViewType name_prefix = {});
```
