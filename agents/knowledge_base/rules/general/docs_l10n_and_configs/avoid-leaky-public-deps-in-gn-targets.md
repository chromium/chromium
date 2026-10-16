---
name: general/avoid-leaky-public-deps-in-gn-targets
rubric: design
severity: should_fix
---

**Rule.** In GN build files (`BUILD.gn` and `*.gni`), explicitly declare a
target's public API headers using the `public` variable rather than leaving all
headers in `sources`, and place private implementation headers in `sources`.
When specific test targets or internal dependents require access to internal
headers in `sources`, grant access using `friend = [ ... ]` rather than
expanding `public`. Restrict `public_deps` strictly to dependencies whose
headers or types are directly `#include`d or exposed by the target's exported
`public` headers, and use private `deps` for all implementation-only
dependencies.

**Why.** Omitting `public` causes GN to treat all headers in `sources` as public
by default, unintentionally widening the public API contract and increasing
recompilation cascades across downstream targets. Overly broad `public_deps`
leak transitively into downstream targets, bloating the dependency graph and
masking missing direct dependencies. Keeping internal implementation headers in
`sources` with private `deps`, and using `friend` to grant selective access only
to authorized internal or test targets, enforces encapsulation and improves
build parallelism.

## Examples

Bad:

```cpp
# Bad: Omitting `public` leaks internal headers, and internal deps are exposed in public_deps.
source_set("third_party_dlls") {
  sources = [
    "beacon.cc",
    "beacon.h",
    "hook.cc",
    "hook.h",
    "main.cc",
    "main.h",
  ]
  public_deps = [
    ":sha1",
    ":third_party_shared_defines",
  ]
}

# Bad: Adding private headers to public so tests can see them instead of using friend.
source_set("feature") {
  public = [
    "feature.h",
    "feature_internal.h",
  ]
  sources = [
    "feature.cc",
    "feature_internal.cc",
  ]
}
```

Good:

```cpp
# Good: Explicit public headers, private implementation in sources, deps vs public_deps split.
source_set("third_party_dlls") {
  public = [
    "beacon.h",
    "main.h",
  ]
  sources = [
    "beacon.cc",
    "hook.cc",
    "hook.h",
    "main.cc",
  ]
  deps = [
    ":sha1",
  ]
  public_deps = [
    ":third_party_shared_defines",
  ]
}

# Good: Keep internal headers in sources and grant test access via friend.
source_set("feature") {
  friend = [
    ":feature_unittests",
  ]
  public = [
    "feature.h",
  ]
  sources = [
    "feature.cc",
    "feature_internal.cc",
    "feature_internal.h",
  ]
  deps = [
    "//base",
  ]
}
```
