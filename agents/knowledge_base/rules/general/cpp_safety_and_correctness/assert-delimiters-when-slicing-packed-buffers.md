---
name: general/assert-delimiters-when-slicing-packed-buffers
rubric: safety
severity: optional
---

**Rule.** When indexing, slicing, or synthesizing sub-views from a packed or
delimited buffer using precomputed (offset, length) pairs, assert delimiter
expectations (such as CHECK_EQ(buffer[offset + length], delimiter) or verifying
the terminating null) before slicing. Enforce boundary invariants to detect
offset corruption and off-by-one errors early.

**Why.** Precomputed offsets and lengths decouple initial parsing from
subsequent view extraction, risking silent corruption or invalid sub-ranges if
indexing state drifts. Asserting delimiter characters at boundary offsets
ensures internal indexing invariants hold and catches slicing bugs
deterministically.

## Examples

Bad:

```cpp
// BAD: Slices into buffer using precomputed offsets without validating delimiter invariants.
std::wstring_view ParsedEnvironmentString::Iterator::operator*() const {
  CHECK(it_ != end_it_);
  return unparsed_view_.substr(it_->offset, it_->length);
}
```

Good:

```cpp
// GOOD: Asserts that the expected terminating delimiter is present at offset + length.
std::wstring_view ParsedEnvironmentString::Iterator::operator*() const {
  CHECK(it_ != end_it_);
  CHECK_EQ(unparsed_view_[it_->offset + it_->length], L'\0');
  return unparsed_view_.substr(it_->offset, it_->length);
}
```
