# Replacing one of two mutually-recursive C++ functions with Rust

This test is based on the Crubit example from
https://github.com/google/crubit/pull/2846.

The test simulates a C++ target where one function has been rewritten in Rust,
but other C++ functions in the same target still call it, and it calls them:

* `parity.h` declares `parity::IsEven` and `parity::IsOdd`, which call each
  other.
* `IsEven` is still implemented in C++ (`parity.cc`).
* `IsOdd` has been rewritten in Rust, as `is_odd::is_odd` (`is_odd.rs`).  It
  calls `IsEven` through `rust_api_from_cpp` bindings
  (`:parity_headers_rs_api`; see
  [`rust_api_from_cpp.md`](/docs/rust/crubit/rust_api_from_cpp.md)).
* `parity.cc` keeps a one-line C++ definition of `parity::IsOdd`.  It calls
  `is_odd::is_odd` through `cpp_api_from_rust` bindings (`:is_odd_cpp_api`;
  see [`cpp_api_from_rust.md`](/docs/rust/crubit/cpp_api_from_rust.md)).  C++
  callers of `parity::IsOdd` don't change.

In Chromium, `:parity` would usually be a big target (e.g. `//base` or
`//content/browser`), and `is_odd.rs` would replace only a small part of it.

## Avoiding the dependency cycle

`:parity` depends on the Rust code, so the Rust code can't depend on `:parity`
(because this would form a dependency cycle).  Instead, the headers of
`:parity` are moved into a separate, headers-only target, and the Rust code
uses the bindings of this headers-only target:

```gn
source_set("parity") {
  sources = [ "parity.cc" ]
  public_deps = [ ":parity_headers" ]
  deps = [ ":is_odd_cpp_api" ]
}

source_set("parity_headers") {
  sources = [ "parity.h" ]
  visibility = [ ":*" ]
}

rust_api_from_cpp("parity_headers_rs_api") {
  bindings_target = ":parity_headers"  # Not `:parity` (that would be a cycle).
  public_headers = [ "parity.h" ]
  visibility = [ ":*" ]
}

rust_static_library("is_odd") {
  crate_root = "is_odd.rs"
  sources = [ crate_root ]
  deps = [ ":parity_headers_rs_api" ]  # Not `:parity` (that would be a cycle).
  cpp_api_from_rust = {
    target_name = "is_odd_cpp_api"
    cpp_namespace = "is_odd"
  }
  visibility = [ ":*" ]
}
```

Notes:

* `:parity` has `public_deps` on `:parity_headers`.  Therefore C++ callers
  can keep depending on `:parity` (and including `parity.h`), and every header
  is listed in only one target.
* `public_deps` and `public_configs` that the headers need belong on
  `:parity_headers` (and `:parity` gets them through its `public_deps`).
  `deps` that only the `.cc` files need stay on `:parity`.  If the headers
  `#include` headers from other targets, then these targets need to be
  `public_deps` of `:parity_headers`.  Otherwise Crubit may not be able to
  parse the headers (because it only gets the `public_configs` that reach it
  through `bindings_target`).
* The definitions come from `:parity`.  Therefore every executable (e.g. an
  `.exe` or a component / `.dll` / `.so`) that uses the Rust code also needs
  to depend on `:parity` (see `:rust_unittests` in `BUILD.gn`).  A target that
  depends only on `:parity_headers` would compile, but would fail to link.
  Therefore `visibility` of `:parity_headers` and of all targets between
  `:parity` and `:parity_headers` (e.g. `:is_odd` and
  `:parity_headers_rs_api`) should be limited to `:parity` and to the targets
  that `:parity` depends on.
* A big target may build its `sources` list in many steps (e.g. in many `if`
  blocks).  In such cases, `filter_include` and `filter_exclude` can split a
  single list into headers and `.cc` files.  For an example, see
  `//base:base_headers` in https://crrev.com/c/8752581.

## Build performance

`:parity` depends on `:is_odd_cpp_api`.  Therefore all `.cc` files of
`:parity` have to wait until the Rust code (and its bindings) is built.
This can make builds slower for big, monolithic targets (and targets that
benefit from the `..._headers` target technique are usually quite big).
[`build_performance.md`](/docs/rust/crubit/build_performance.md) explains how
to avoid this (e.g. by moving the `.cc` files that `#include` the bindings
into a separate `source_set` with `public = []`).

## Components

If the original target is a `component` (e.g. `//base`), then the Rust code
(and the C++ code generated for it by Crubit or by `cxx`) is linked into this
component.  This C++ code is compiled by other targets, but it needs the same
`..._IMPLEMENTATION` define as the component (e.g. `BASE_IMPLEMENTATION`).
Otherwise, in component builds on Windows, `..._EXPORT` macros would mean
`__declspec(dllimport)` (rather than `__declspec(dllexport)`) in this code.

One way to do this is to put the define into a `config` with narrow
`visibility`, and to apply this `config`:

* To the Rust target (e.g. `configs += [ "//base:base_implementation" ]`).
  This covers the C++ code that `cxx` generates.  For an example, see
  `//base/logging/rust_logger` in https://crrev.com/c/8752581.
* To the C++ thunks that `rust_api_from_cpp` generates (e.g. for inline
  functions).  `rust_api_from_cpp` uses the `public_configs` of
  `bindings_target`, so `bindings_target` can be a small, narrowly visible
  target with `public_configs = [ ":..._implementation" ]` and `public_deps`
  on the headers-only target.  For an example, see `orb_sniffers_headers_impl`
  in https://crrev.com/c/8794257.

## See also

The "Dependency cycles and missing dependencies" section in
[`//docs/rust/crubit/rust_api_from_cpp.md`](/docs/rust/crubit/rust_api_from_cpp.md).
