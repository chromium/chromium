---
name: general/consolidate-duplicate-gn-conditionals
rubric: readability
severity: should_fix
---

**Rule.** In GN build files (`.gn`, `.gni`), do not introduce duplicate
conditional blocks for the same condition within a target or template.
Consolidate additions to `sources`, `deps`, `configs`, and other variables into
existing matching conditional blocks.

**Why.** The GN Style Guide prescribes minimizing the number of conditional
blocks. Consolidating variable additions under unified condition blocks prevents
build target fragmentation, keeps related platform configuration cohesive, and
makes target definitions easier to read and maintain.

## Examples

Bad:

```cpp
if (!is_android && !is_chromeos) {
  sources += [ "$root_gen_dir/chrome/default_browser_modal_resources.pak" ]
  deps += [ "//chrome/browser/resources/default_browser/default_browser_modal:resources" ]
}
...
if (!is_android && !is_chromeos) {
  sources += [
    "$root_gen_dir/chrome/intro_resources.pak",
    "$root_gen_dir/chrome/profile_picker_resources.pak",
  ]
  deps += [
    "//chrome/browser/resources/intro:resources",
    "//chrome/browser/resources/signin/profile_picker:resources",
  ]
}
```

Good:

```cpp
if (!is_android && !is_chromeos) {
  sources += [
    "$root_gen_dir/chrome/default_browser_modal_resources.pak",
    "$root_gen_dir/chrome/intro_resources.pak",
    "$root_gen_dir/chrome/profile_picker_resources.pak",
  ]
  deps += [
    "//chrome/browser/resources/default_browser/default_browser_modal:resources",
    "//chrome/browser/resources/intro:resources",
    "//chrome/browser/resources/signin/profile_picker:resources",
  ]
}
```
