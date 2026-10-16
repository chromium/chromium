---
name: general/encapsulate-unsafe-ffi-conversions-in-from-into-traits
rubric: readability
severity: should_fix
---

**Rule.** Implement standard conversion traits (`From`/`Into` or
`TryFrom`/`TryInto`) to convert between Rust types and equivalent FFI types.
Encapsulate any unsafe conversions (such as ABI-compatible `transmute` or
pointer casts) inside the trait implementation alongside a dedicated
`// SAFETY:` comment rather than scattering `unsafe` blocks and inline
conversions across caller methods.

**Why.** Centralizing type conversions in standard conversion traits isolates
`unsafe` blocks and safety justifications in a single audited location. Callers
can then perform conversions safely and idiomatically using `.into()` or
`From::from()` without duplicating unsafe boilerplate or risking unvalidated
casts.

## Examples

Bad:

```cpp
pub fn is_enabled(&self) -> bool {
    // Bad: scattering unsafe transmutes / FFI casts across caller methods.
    unsafe {
        let f: &ffi::Feature = std::mem::transmute(self);
        ffi::IsFeatureEnabled(f)
    }
}
```

Good:

```cpp
pub fn is_enabled(&self) -> bool {
    // Good: call .into() safely and idiomatically at call sites.
    ffi::Feature::is_enabled(self.into())
}

impl<'a> From<&'a Feature> for &'a ffi::Feature {
    fn from(feature: &'a Feature) -> Self {
        // SAFETY: Feature is ABI-compatible with ffi::Feature (verified by
        // static assertions).
        unsafe { std::mem::transmute(feature) }
    }
}
```
