---
name: general/use-standard-rust-assertion-macros
rubric: readability
severity: optional
---

**Rule.** In Chromium Rust code, use standard assertion and panic macros
(`debug_assert!` for C++ `DCHECK`, `assert!` for `CHECK`, and `panic!` or
`unreachable!` for `NOTREACHED()`). Avoid implementing custom
error-logging-and-abort paths or ad-hoc crash handling for invariant violations.

**Why.** Chromium's build system configures `dcheck_always_on` consistently
across C++ and Rust (enabling `debug_assertions`), and Chromium's custom panic
hook routes Rust panics directly to `LOG(FATAL)`. Using standard assertion
macros ensures uniform stack traces, crash keys, and test-harness failure
reporting across languages.

## Examples

Bad:

```cpp
fn process_item(ready: bool, count: usize) {
    if !ready {
        log::error!("Item not ready");
        std::process::abort();
    }
    if count == 0 {
        // Custom unreachable handling
        log::error!("Unexpected empty count");
        return;
    }
}
```

Good:

```cpp
fn process_item(ready: bool, count: usize) {
    // debug_assert! is enabled under dcheck_always_on and routes to LOG(FATAL).
    debug_assert!(ready, "Item not ready");
    // assert! is always enabled, equivalent to CHECK().
    assert!(count > 0, "Unexpected empty count");
}
```
