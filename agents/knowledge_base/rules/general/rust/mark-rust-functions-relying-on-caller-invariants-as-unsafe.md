---
name: general/mark-rust-functions-relying-on-caller-invariants-as-unsafe
rubric: safety
severity: should_fix
---

**Rule.** In Rust code, declare functions as unsafe fn (and traits as unsafe
trait) and document their requirements under a '/// # Safety' doc comment
whenever safe execution relies on unchecked caller preconditions or invariants
(such as pointer validity, non-nullness, or buffer lengths) to prevent undefined
behavior. Do not rely on macro callers, module encapsulation, or #[doc(hidden)]
to ensure soundness without an explicit unsafe declaration.

**Why.** In Rust, safe functions must be sound for all possible safe arguments.
If a function can trigger undefined behavior when callers violate unasserted
preconditions, omitting unsafe fn breaks language soundness guarantees and
prevents callers and macros from auditing and documenting safety requirements in
unsafe blocks.

## Examples

Bad:

```cpp
#[doc(hidden)]
pub const fn with_derived_name(name_with_k: &'static str) -> Self {
    // Bad: Relying on caller/macro invariants without an unsafe fn declaration.
    Self {
        name: unsafe { name_with_k.as_ptr().add(1) as *const c_char },
    }
}
```

Good:

```cpp
/// # Safety
/// `name_with_k` must be a null-terminated string at least 2 bytes long,
/// starting with 'k'.
#[doc(hidden)]
pub const unsafe fn with_derived_name(name_with_k: &'static str) -> Self {
    Self {
        name: unsafe { name_with_k.as_ptr().add(1) as *const c_char },
    }
}

// In macro caller:
static $id: $crate::Feature = unsafe {
    // SAFETY: `$name` starts with "k\0" and is at least 2 bytes long.
    $crate::Feature::with_derived_name(...)
};
```
