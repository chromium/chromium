---
name: general/hide-macro-internal-rust-modules-with-doc-hidden
rubric: readability
severity: nit
---

**Rule.** When defining public modules, functions, traits, or types in Rust that
exist solely to support macro expansion or crate-internal mechanics, annotate
the enclosing module or item with `#[doc(hidden)]`. Prefer placing
`#[doc(hidden)]` on the enclosing `internal` module rather than repeating the
attribute on every individual item inside.

**Why.** Exported macros often require supporting helper types or functions to
have `pub` visibility so downstream callers can expand them across module or
crate boundaries. Annotating the enclosing module with `#[doc(hidden)]` hides
internal implementation machinery from the public `rustdoc` API documentation
and prevents cluttering the crate's documented interface.

## Examples

Bad:

```cpp
// Missing #[doc(hidden)] on enclosing module clutters public rustdoc.
pub mod internal {
    #[doc(hidden)]
    pub enum FeatureMacroHandshake {
        #[doc(hidden)]
        Secret,
    }
}
```

Good:

```cpp
// #[doc(hidden)] on the module hides all internal macro helpers from rustdoc.
#[doc(hidden)]
pub mod internal {
    pub enum FeatureMacroHandshake {
        Secret,
    }
}
```
