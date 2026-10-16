---
name: general/require-safety-comments-for-unsafe-rust-code
rubric: readability
severity: should_fix
---

**Rule.** In first-party Rust code, accompany every `unsafe` block or
`unsafe impl` with an explanatory safety comment (`// SAFETY:` or `// Safety:`)
detailing how preconditions are satisfied and why the operation is sound, and
document caller preconditions for `unsafe fn` or `unsafe trait` under a
`# Safety` doc comment. When invariants or preconditions are enforced by
compile-time checks (such as FFI layout or ABI compatibility), state them
definitively as established facts rather than aspirational requirements (e.g.,
'is ABI-compatible' rather than 'must be ABI-compatible') and explicitly
reference the static assertions that verify them.

**Why.** Unsafe Rust bypasses compiler memory-safety and type-checking
guarantees, risking undefined behavior and memory corruption. Explicit safety
comments ensure reviewers and maintainers can verify soundness contracts, while
stating compile-time verified invariants as facts and citing static assertions
eliminates ambiguity over whether safety requirements are actively enforced.

## Examples

Bad:

```cpp
pub fn is_enabled(&self) -> bool {
    // Unsafe block without an explanatory safety comment
    unsafe {
        let f: &ffi::Feature = std::mem::transmute(self);
        ffi::IsFeatureEnabled(f)
    }
}

// Aspirational comment without citing compile-time assertions
// Safety: Must be ABI-compatible with the C++ base::Feature.
unsafe { std::mem::transmute(feature) }

pub unsafe fn deref_ptr(ptr: *const u32) -> u32 {
    // Missing '# Safety' doc comment on unsafe function declaration
    *ptr
}
```

Good:

```cpp
impl<'a> From<&'a Feature> for &'a ffi::Feature {
    fn from(feature: &'a Feature) -> Self {
        // SAFETY: Feature is ABI-compatible with ffi::Feature (checked by
        // static asserts above).
        unsafe { std::mem::transmute(feature) }
    }
}

/// # Safety
/// `ptr` must be non-null, properly aligned, and point to an initialized `u32`.
pub unsafe fn deref_ptr(ptr: *const u32) -> u32 {
    // SAFETY: Caller guarantees pointer validity per the # Safety contract.
    *ptr
}
```
