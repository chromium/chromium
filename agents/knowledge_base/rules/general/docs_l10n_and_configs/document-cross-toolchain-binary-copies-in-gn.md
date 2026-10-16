---
name: general/document-cross-toolchain-binary-copies-in-gn
rubric: readability
severity: nit
---

**Rule.** In GN build files (`.gn`, `.gni`), when declaring a `copy()` target to
forward or copy binaries across toolchains into the root output directory
(`root_out_dir`), add explanatory comments. Document why the copy target is
needed (specifically that the alternate toolchain places build outputs in a
toolchain-specific subdirectory rather than the main output directory).

**Why.** Targets compiled with a secondary or alternate toolchain place their
outputs in a toolchain-specific subfolder rather than the main `root_out_dir`.
Downstream test harnesses and packaging steps look for binaries in the root
output directory; without explanatory comments, defining a `copy()` target
alongside an `executable()` with identical names across `if/else` toolchain
branches appears redundant or confusing to maintainers.

## Examples

Bad:

```cpp
if (is_win || is_linux) {
  if (current_toolchain == default_system_allocator_toolchain) {
    executable("system_allocator_test_helper") {
      testonly = true
      sources = [ "system_allocator_test_helper.cc" ]
      deps = [ "//base" ]
    }
  } else {
    copy("system_allocator_test_helper") {
      testonly = true
      _toolchain = default_system_allocator_toolchain
      _label = ":system_allocator_test_helper($_toolchain)"
      _dir = get_label_info(_label, "root_out_dir")
      _name = get_label_info(_label, "name")
      sources = [ "$_dir/$_name" ]
      if (target_os == "win") {
        sources[0] += ".exe"
        sources += [ "$_dir/$_name.exe.pdb" ]
      }
      outputs = [ "$root_out_dir/{{source_file_part}}" ]
      deps = [ _label ]
    }
  }
}
```

Good:

```cpp
if (is_win || is_linux) {
  if (current_toolchain == default_system_allocator_toolchain) {
    # This binary ends up in the 'system_allocator' build output directory.
    executable("system_allocator_test_helper") {
      testonly = true
      sources = [ "system_allocator_test_helper.cc" ]
      deps = [ "//base" ]
    }
  } else {
    # This copies the binary (and symbols) from the 'system_allocator' build
    # output directory into the main build output dir.
    copy("system_allocator_test_helper") {
      testonly = true
      _toolchain = default_system_allocator_toolchain
      _label = ":system_allocator_test_helper($_toolchain)"
      _dir = get_label_info(_label, "root_out_dir")
      _name = get_label_info(_label, "name")
      sources = [ "$_dir/$_name" ]
      if (target_os == "win") {
        sources[0] += ".exe"
        sources += [ "$_dir/$_name.exe.pdb" ]
      }
      outputs = [ "$root_out_dir/{{source_file_part}}" ]
      deps = [ _label ]
    }
  }
}
```
