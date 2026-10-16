---
name: general/no-redundant-to-string-on-owned-string
rubric: safety
severity: nit
---

**Rule.** In Rust code, do not call `.to_string()`, `.to_owned()`, or `.clone()`
on expressions or functions that already produce an owned `String`. Pass or
return the owned `String` directly to avoid redundant heap allocations and
character buffer copies.

**Why.** Calling `.to_string()` on a value that is already an owned `String`
invokes `ToString::to_string(&s)` via deref coercion to `&str`, which allocates
a duplicate buffer on the heap and copies all characters unnecessarily.
Transferring the owned `String` directly avoids heap allocation overhead.

## Examples

Bad:

```cpp
// Redundant .to_string() on an expression that already returns String.
pub fn transliterate_text(t: &Transliterator, text: &str) -> String {
    return t.0.transliterate(text.to_string()).to_string();
}
```

Good:

```cpp
// Return the owned String directly without allocating a duplicate buffer.
pub fn transliterate_text(t: &Transliterator, text: &str) -> String {
    return t.0.transliterate(text.to_string());
}
```
