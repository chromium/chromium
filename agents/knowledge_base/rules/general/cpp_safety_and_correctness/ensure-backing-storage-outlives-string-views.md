---
name: general/ensure-backing-storage-outlives-string-views
rubric: safety
severity: should_fix
---

**Rule.** Ensure the backing storage of a std::string_view, base::cstring_view,
or base::span outlives every usage of the view. Do not assign to an outer-scoped
non-owning view from a local or temporary object (such as a base::FilePath or
std::string) whose lifetime ends at an inner block boundary or temporary
expression before the view is consumed.

**Why.** Non-owning view types like std::string_view and base::span do not
extend the lifetime of underlying character buffers. If the underlying owning
object is destroyed at the end of an inner block, the view is left dangling,
causing use-after-free and memory corruption bugs when accessed.

## Examples

Bad:

```cpp
std::optional<std::wstring_view> current_directory;
if (!current_directory) {
  base::FilePath current_dir;
  if (base::PathService::Get(base::DIR_CURRENT, &current_dir)) {
    // BUG: current_dir is destroyed at block exit, leaving current_directory dangling.
    current_directory = current_dir.value();
  }
}
UseDirectory(current_directory->data());
```

Good:

```cpp
std::optional<std::wstring_view> current_directory;
std::optional<base::FilePath> current_dir;
if (!current_directory) {
  current_dir = base::PathService::CheckedGet(base::DIR_CURRENT);
  current_directory = current_dir->value();
}
// current_dir remains in scope and alive across all usages of current_directory.
UseDirectory(current_directory->data());
```
